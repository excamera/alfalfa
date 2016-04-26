#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>

#include "frame.hh"
#include "player.hh"
#include "vp8_raster.hh"
#include "decoder.hh"
#include "macroblock.hh"
#include "ivf_writer.hh"
#include "display.hh"

using namespace std;

KeyFrame make_empty_frame( unsigned int width, unsigned int height ) {
  BoolDecoder data { { nullptr, 0 } };
  KeyFrame frame { true, width, height, data };
  frame.parse_macroblock_headers( data, ProbabilityTables {} );
  return frame;
}

template <class FrameHeaderType2, class MacroblockHeaderType2>
void copy_macroblock( Macroblock<FrameHeaderType2, MacroblockHeaderType2> & target,
                      const Macroblock<FrameHeaderType2, MacroblockHeaderType2> & source,
                      const Quantizer & quantizer )
{
  target.segment_id_update_ = source.segment_id_update_;
  target.segment_id_ = source.segment_id_;
  target.mb_skip_coeff_ = source.mb_skip_coeff_;
  target.header_ = source.header_;
  target.has_nonzero_ = source.has_nonzero_;

  /* copy contents */
  target.U_.copy_from( source.U_ );
  target.V_.copy_from( source.V_ );

  /* copy Y */
  target.Y_.forall_ij( [&] ( YBlock & target_block, const unsigned int col, const unsigned int row ) {
      const YBlock & source_block = source.Y_.at( col, row );

      target_block.set_prediction_mode( source_block.prediction_mode() );
      target_block.set_motion_vector( source_block.motion_vector() );
      assert( source_block.motion_vector() == MotionVector() );
      auto dequantized_coeffs = source_block.dequantize( quantizer );
      target_block.mutable_coefficients() = YBlock::quantize( quantizer, dequantized_coeffs );
      target_block.calculate_has_nonzero();

      if ( source_block.type() == Y_without_Y2 ) {
        target_block.set_Y_without_Y2();
      } else if ( source_block.type() == Y_after_Y2 ) {
        target_block.set_Y_after_Y2();
      }

      assert( target_block.coefficients() == source_block.coefficients() );
      assert( target_block.type() == source_block.type() );
      assert( target_block.prediction_mode() == source_block.prediction_mode() );
      assert( target_block.has_nonzero() == source_block.has_nonzero() );
      assert( target_block.motion_vector() == source_block.motion_vector() );
    } );

    /* copy Y2 */
    target.Y2_.set_prediction_mode( source.Y2_.prediction_mode() );
    target.Y2_.set_coded( source.Y2_.coded() );

    if ( target.Y2_.coded() ) {
      auto dequantize_wht_coeffs = source.Y2_.dequantize( quantizer );
      auto inversed_wht_coeffs = dequantize_wht_coeffs.iwht();
      DCTCoefficients wht_back;
      wht_back.wht( inversed_wht_coeffs );
      target.Y2_.mutable_coefficients() = Y2Block::quantize( quantizer, wht_back );
    }

    target.Y2_.calculate_has_nonzero();
}

template <>
void copy_frame( KeyFrame & target, const KeyFrame & source,
                 const Optional<Segmentation> & segmentation )
{
  target.show_ = source.show_;
  target.header_ = source.header_;

  assert( target.display_width_ == source.display_width_ );
  assert( target.display_height_ == source.display_height_ );
  assert( target.macroblock_width_ == source.macroblock_width_ );
  assert( target.macroblock_height_ == source.macroblock_height_ );

  assert( source.macroblock_headers_.initialized() );
  assert( target.macroblock_headers_.initialized() );

  assert( source.macroblock_headers_.get().width() == target.macroblock_headers_.get().width() );
  assert( source.macroblock_headers_.get().height() == target.macroblock_headers_.get().height() );

  /* copy each macroblock */
  const Quantizer frame_quantizer( target.header_.quant_indices );
  const auto segment_quantizers = target.calculate_segment_quantizers( segmentation );

  target.macroblock_headers_.get().forall_ij( [&] ( KeyFrameMacroblock & target_macroblock,
                                                    const unsigned int col,
                                                    const unsigned int row ) {
                                                const auto & quantizer = segmentation.initialized()
                                                  ? segment_quantizers.at( source.macroblock_headers_.get().at( col, row ).segment_id() )
                                                  : frame_quantizer;

                                                copy_macroblock( target_macroblock,
                                                                 source.macroblock_headers_.get().at( col, row ),
                                                                 quantizer );
                                              } );

  target.relink_y2_blocks();
}

vector<uint8_t> loopback( const Chunk & chunk, const uint16_t width, const uint16_t height )
{
    vector<uint8_t> serialized_new_frame;

    UncompressedChunk temp_uncompressed_chunk { chunk, width, height };
    if ( not temp_uncompressed_chunk.key_frame() ) {
      throw runtime_error( "not a keyframe" );
    }

    DecoderState temp_state { width, height };
    KeyFrame frame = temp_state.parse_and_apply<KeyFrame>( temp_uncompressed_chunk );
    vector<uint8_t> serialized_old_frame = frame.serialize( temp_state.probability_tables );

    KeyFrame temp_new_frame = make_empty_frame( width, height );
    copy_frame( temp_new_frame, frame,
                temp_state.segmentation );

    assert( temp_new_frame == frame );

    serialized_new_frame = temp_new_frame.serialize( temp_state.probability_tables );

    if ( serialized_new_frame != serialized_old_frame ) {
      cerr << "roundtrip failure!" << endl;
      // throw runtime_error( "roundtrip failure" );
    }

    return serialized_new_frame;
}

template <unsigned int size>
template <class PredictionMode>
void VP8Raster::Block<size>::intra_predict( const PredictionMode mb_mode, TwoD< uint8_t > & output )
{
  TwoDSubRange< uint8_t, size, size > subrange( output, 0, 0 );
  intra_predict( mb_mode, subrange );
}

vector< uint8_t > encode( const VP8Raster & raster )
{
  unsigned int width = raster.width();
  unsigned int height = raster.height();

  KeyFrame frame = make_empty_frame( width, height );
  DecoderState temp_state { width, height };
  Quantizer quantizer( frame.header().quant_indices );

  raster.macroblocks().forall_ij( [&] ( VP8Raster::Macroblock & raster_macroblock,
    unsigned int column_mb, unsigned int row_mb ) {
      auto & frame_macroblock = frame.mutable_macroblocks().at( column_mb, row_mb );
      // Process Y
      frame_macroblock.Y2_.set_prediction_mode( B_PRED ); // predict each subblock separately

      raster_macroblock.Y_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_y_subblock,
        unsigned int column_sb, unsigned int row_sb ) {
          YBlock & frame_y_subblock = frame_macroblock.Y_.at( column_sb, row_sb );

          TwoD< uint8_t > prediction( 4, 4 );
          raster_y_subblock.intra_predict( B_DC_PRED, prediction );
          frame_y_subblock.set_prediction_mode( B_DC_PRED );
          frame_y_subblock.mutable_coefficients().subtract_dct( raster_y_subblock,
                                                                TwoDSubRange< uint8_t, 4, 4 >( prediction, 0, 0 ) );
          frame_y_subblock.mutable_coefficients() = YBlock::quantize( quantizer, frame_y_subblock.coefficients() );

          frame_y_subblock.set_Y_without_Y2();
          frame_y_subblock.calculate_has_nonzero();
        } );

        // Process Y2
        // Not needed for now
        frame_macroblock.Y2_.set_coded( false );

        // Process U
        frame_macroblock.U_.at( 0, 0 ).set_prediction_mode( V_PRED );
        TwoD< uint8_t > prediction( 8, 8 );

        raster_macroblock.U.intra_predict( V_PRED, prediction );
        raster_macroblock.U_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_u_subblock,
          unsigned int column_sb, unsigned int row_sb ) {
            UVBlock & frame_u_subblock = frame_macroblock.U_.at( column_sb, row_sb );

            frame_u_subblock.mutable_coefficients().subtract_dct( raster_u_subblock,
                                                                  TwoDSubRange< uint8_t, 4, 4 >( prediction, 4 * column_sb, 4 * row_sb ) );
            frame_u_subblock.mutable_coefficients() = UVBlock::quantize( quantizer, frame_u_subblock.coefficients() );
            frame_u_subblock.calculate_has_nonzero();
          } );

          // Process V
          raster_macroblock.V.intra_predict( V_PRED, prediction );
          raster_macroblock.V_sub.forall_ij( [&] ( VP8Raster::Block4 & raster_v_subblock,
            unsigned int column_sb, unsigned int row_sb ) {
              UVBlock & frame_v_subblock = frame_macroblock.V_.at( column_sb, row_sb );

              frame_v_subblock.mutable_coefficients().subtract_dct( raster_v_subblock,
                                                                    TwoDSubRange< uint8_t, 4, 4 >( prediction, 4 * column_sb, 4 * row_sb ) );
              frame_v_subblock.mutable_coefficients() = UVBlock::quantize( quantizer, frame_v_subblock.coefficients() );
              frame_v_subblock.calculate_has_nonzero();
            } );
      } );

    frame.relink_y2_blocks();
    return frame.serialize( temp_state.probability_tables );
}

int main( int argc, char *argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 2 ) {
      cerr << "Usage: " << argv[ 0 ] << " <video>" << endl;
      return EXIT_FAILURE;
    }

    const IVF ivf( argv[ 1 ] );

    FramePlayer player { ivf.width(), ivf.height() };
    FilePlayer file_player { argv[ 1 ] };
    VideoDisplay display { player.example_raster() };

    for ( unsigned int frame_number = 0; frame_number < ivf.frame_count(); frame_number++ ) {
      auto raster = file_player.advance();
      vector< uint8_t > f = encode( raster );

      const auto maybe_raster = player.decode( f );

      if ( maybe_raster.initialized() ) {
        display.draw( maybe_raster.get() );
      }
    }
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
