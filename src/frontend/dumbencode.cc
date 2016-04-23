#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>

#include "dct.hh"
#include "frame.hh"
#include "player.hh"
#include "vp8_raster.hh"
#include "decoder.hh"
#include "display.hh"
#include "macroblock.hh"

using namespace std;

KeyFrame make_empty_frame( unsigned int width, unsigned int height ) {
  BoolDecoder data { { nullptr, 0 } };
  KeyFrame frame { true, width, height, data };
  frame.parse_macroblock_headers( data, ProbabilityTables {} );
  return frame;
}

template <class FrameHeaderType2, class MacroblockHeaderType2>
void copy_macroblock( Macroblock<FrameHeaderType2, MacroblockHeaderType2> & target,
		      const Macroblock<FrameHeaderType2, MacroblockHeaderType2> & source )
{
  target.segment_id_update_ = source.segment_id_update_;
  target.segment_id_ = source.segment_id_;
  target.mb_skip_coeff_ = source.mb_skip_coeff_;
  target.header_ = source.header_;
  target.has_nonzero_ = source.has_nonzero_;
}

template <>
void copy_frame( KeyFrame & target, const KeyFrame & source )
{
  target.show_ = source.show_;
  assert( target.display_width_ == source.display_width_ );
  assert( target.display_height_ == source.display_height_ );
  assert( target.macroblock_width_ == source.macroblock_width_ );
  assert( target.macroblock_height_ == source.macroblock_height_ );

  target.Y2_.copy_from( source.Y2_ );
  target.Y_.copy_from( source.Y_ );
  target.U_.copy_from( source.U_ );
  target.V_.copy_from( source.V_ );

  target.header_ = source.header_;

  assert( source.macroblock_headers_.initialized() );
  assert( target.macroblock_headers_.initialized() );

  assert( source.macroblock_headers_.get().width() == target.macroblock_headers_.get().width() );
  assert( source.macroblock_headers_.get().height() == target.macroblock_headers_.get().height() );

  /* copy each macroblock */
  target.macroblock_headers_.get().forall_ij( [&] ( KeyFrameMacroblock & target_macroblock,
						    const unsigned int col,
						    const unsigned int row ) {
						copy_macroblock( target_macroblock,
								 source.macroblock_headers_.get().at( col, row ) );
					      } );

  target.relink_y2_blocks();
}

int main( int argc, char *argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <video> <frame number>" << endl;
      return EXIT_FAILURE;
    }

    vector<uint8_t> serialized_new_frame;
    uint16_t width, height;

    {
      const IVF ivf( argv[ 1 ] );
      const unsigned int frame_number = atoi( argv[ 2 ] );

      width = ivf.width();
      height = ivf.height();

      UncompressedChunk temp_uncompressed_chunk { ivf.frame( frame_number ), width, height };
      if ( not temp_uncompressed_chunk.key_frame() ) {
	throw runtime_error( "not a keyframe" );
      }

      DecoderState temp_state { width, height };
      KeyFrame frame = temp_state.parse_and_apply<KeyFrame>( temp_uncompressed_chunk );
      vector<uint8_t> serialized_old_frame = frame.serialize( temp_state.probability_tables );

      KeyFrame temp_new_frame = make_empty_frame( width, height );
      copy_frame( temp_new_frame, frame );

      assert( temp_new_frame == frame );

      serialized_new_frame = temp_new_frame.serialize( temp_state.probability_tables );

      cerr << "length of original frame: " << ivf.frame( frame_number ).size() << "\n";
      cerr << "length of original frame, serialized: " << serialized_old_frame.size() << "\n";
      cerr << "length of new frame: " << serialized_new_frame.size() << "\n";

      assert( serialized_new_frame == serialized_old_frame );
    }

    /* new decode the copied frame */
    UncompressedChunk uncompressed_chunk { serialized_new_frame, width, height };
    DecoderState state { width, height };
    KeyFrame new_frame = state.parse_and_apply<KeyFrame>( uncompressed_chunk );

    References references { width, height };
    MutableRasterHandle raster { width, height };

    new_frame.decode( state.segmentation, references, raster );
    new_frame.loopfilter( state.segmentation, state.filter_adjustments, raster );

    VideoDisplay display { raster };
    display.draw( raster );

    pause();
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
