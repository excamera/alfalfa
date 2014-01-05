#include <vector>

#include "macroblock.hh"
#include "decoder.hh"

#include "tree.cc"
#include "tokens.cc"
#include "transform.cc"
#include "prediction.cc"
#include "quantization.cc"

using namespace std;

static bmode implied_subblock_mode( const mbmode y_mode )
{
  switch ( y_mode ) {
  case DC_PRED: return B_DC_PRED;
  case V_PRED:  return B_VE_PRED;
  case H_PRED:  return B_HE_PRED;
  case TM_PRED: return B_TM_PRED;
  default: assert( false ); return bmode();
  }
}

template <class FrameHeaderType, class MacroblockHeaderType>
Macroblock<FrameHeaderType, MacroblockHeaderType>::Macroblock( const typename TwoD< Macroblock >::Context & c,
							       BoolDecoder & data,
							       const FrameHeaderType & frame_header,
							       const DecoderState & decoder_state,
							       TwoD< Y2Block > & frame_Y2,
							       TwoD< YBlock > & frame_Y,
							       TwoD< UVBlock > & frame_U,
							       TwoD< UVBlock > & frame_V )
  : header_( data, frame_header, decoder_state ),
    Y2_( frame_Y2.at( c.column, c.row ) ),
    Y_( frame_Y, c.column * 4, c.row * 4 ),
    U_( frame_U, c.column * 2, c.row * 2 ),
    V_( frame_V, c.column * 2, c.row * 2 )
{
  decode_prediction_modes( data, decoder_state );
}

template <>
void KeyFrameMacroblock::decode_prediction_modes( BoolDecoder & data,
						  const DecoderState & decoder_state __attribute((unused)) )
{
  /* Set Y prediction mode */
  Y2_.set_prediction_mode( data.tree< num_y_modes, mbmode >( kf_y_mode_tree, kf_y_mode_probs ) );
  Y2_.set_if_coded();

  /* Set subblock prediction modes */
  Y_.forall( [&]( YBlock & block )
	     {
	       if ( Y2_.prediction_mode() == B_PRED ) {
		 const auto above_mode = block.context().above.initialized()
		   ? block.context().above.get()->prediction_mode() : B_DC_PRED;
		 const auto left_mode = block.context().left.initialized()
		   ? block.context().left.get()->prediction_mode() : B_DC_PRED;
		 block.set_Y_without_Y2();
		 block.set_prediction_mode( data.tree< num_intra_b_modes, bmode >( b_mode_tree,
										   kf_b_mode_probs.at( above_mode ).at( left_mode ) ) );
	       } else {
		 block.set_prediction_mode( implied_subblock_mode( Y2_.prediction_mode() ) );
	       }
	     } );

  /* Set chroma prediction mode */
  U_.at( 0, 0 ).set_prediction_mode( data.tree< num_uv_modes, mbmode >( uv_mode_tree, kf_uv_mode_probs ) );
}

template <>
void InterFrameMacroblock::decode_prediction_modes( BoolDecoder & data,
						    const DecoderState & decoder_state )
{
  if ( not header_.is_inter_mb ) {
    /* Set Y prediction mode */
    Y2_.set_prediction_mode( data.tree< num_y_modes, mbmode >( y_mode_tree, decoder_state.y_mode_probs ) );
    Y2_.set_if_coded();

    /* Set subblock prediction modes. Intra macroblocks in interframes are simpler than in keyframes. */
    Y_.forall( [&]( YBlock & block )
	       {
		 if ( Y2_.prediction_mode() == B_PRED ) {
		   block.set_Y_without_Y2();
		   block.set_prediction_mode( data.tree< num_intra_b_modes, bmode >( b_mode_tree,
										     decoder_state.b_mode_probs ) );
		 } else {
		   block.set_prediction_mode( implied_subblock_mode( Y2_.prediction_mode() ) );
		 }
	       } );

    /* Set chroma prediction modes */
    U_.at( 0, 0 ).set_prediction_mode( data.tree< num_uv_modes, mbmode >( uv_mode_tree,
									  decoder_state.uv_mode_probs ) );
  }
}

KeyFrameMacroblockHeader::KeyFrameMacroblockHeader( BoolDecoder & data,
						    const KeyFrameHeader & frame_header,
						    const DecoderState & decoder_state )
  : segment_id( frame_header.update_segmentation.initialized()
		and frame_header.update_segmentation.get().update_mb_segmentation_map,
		data, decoder_state.mb_segment_tree_probs ),
    mb_skip_coeff( frame_header.prob_skip_false.initialized(),
		   data, frame_header.prob_skip_false.get() )
{}

InterFrameMacroblockHeader::InterFrameMacroblockHeader( BoolDecoder & data,
							const InterFrameHeader & frame_header,
							const DecoderState & decoder_state )
  : segment_id( frame_header.update_segmentation.initialized()
		and frame_header.update_segmentation.get().update_mb_segmentation_map,
		data, decoder_state.mb_segment_tree_probs ),
    mb_skip_coeff( frame_header.prob_skip_false.initialized(),
		   data, frame_header.prob_skip_false.get() ),
    is_inter_mb( data, frame_header.prob_inter ),
    mb_ref_frame_sel1( is_inter_mb, data, frame_header.prob_references_last ),
    mb_ref_frame_sel2( mb_ref_frame_sel1.get_or( false ), data, frame_header.prob_references_golden )
{}

template <class FrameHeaderType, class MacroblockHeaderType>
void Macroblock<FrameHeaderType, MacroblockHeaderType>::parse_tokens( BoolDecoder & data,
								      const DecoderState & decoder_state )
{
  /* is macroblock skipped? */
  if ( header_.mb_skip_coeff.get_or( false ) ) {
    return;
  }

  /* parse Y2 block if present */
  if ( Y2_.coded() ) {
    Y2_.parse_tokens( data, decoder_state );
    has_nonzero_ |= Y2_.has_nonzero();
  }

  /* parse Y blocks with variable first coefficient */
  Y_.forall( [&]( YBlock & block ) {
      block.parse_tokens( data, decoder_state );
      has_nonzero_ |= block.has_nonzero(); } );

  /* parse U and V blocks */
  U_.forall( [&]( UVBlock & block ) {
      block.parse_tokens( data, decoder_state );
      has_nonzero_ |= block.has_nonzero(); } );
  V_.forall( [&]( UVBlock & block ) {
      block.parse_tokens( data, decoder_state );
      has_nonzero_ |= block.has_nonzero(); } );
}

template <class FrameHeaderType, class MacroblockHeaderType>
void Macroblock<FrameHeaderType, MacroblockHeaderType>::dequantize( const Quantizer & frame_quantizer,
								    const SafeArray< Quantizer, num_segments > & segment_quantizers )
{
  /* is macroblock skipped? */
  if ( not has_nonzero_ ) {
    return;
  }

  /* which quantizer are we using? */
  const Quantizer & the_quantizer( header_.segment_id.initialized()
				   ? segment_quantizers.at( header_.segment_id.get() )
				   : frame_quantizer );

  if ( Y2_.coded() ) {
    Y2_.dequantize( the_quantizer );
  }

  Y_.forall( [&] ( YBlock & block ) { block.dequantize( the_quantizer ); } );
  U_.forall( [&] ( UVBlock & block ) { block.dequantize( the_quantizer ); } );
  V_.forall( [&] ( UVBlock & block ) { block.dequantize( the_quantizer ); } );
}

template <class FrameHeaderType, class MacroblockHeaderType>
void Macroblock<FrameHeaderType, MacroblockHeaderType>::intra_predict_and_inverse_transform( Raster::Macroblock & raster ) const
{
  const bool do_idct = has_nonzero_;

  /* Chroma */
  raster.U.intra_predict( uv_prediction_mode() );
  raster.V.intra_predict( uv_prediction_mode() );

  if ( do_idct ) {
    U_.forall_ij( [&] ( const UVBlock & block, const unsigned int column, const unsigned int row )
		  { block.idct( raster.U_sub.at( column, row ) ); } );
    V_.forall_ij( [&] ( const UVBlock & block, const unsigned int column, const unsigned int row )
		  { block.idct( raster.V_sub.at( column, row ) ); } );
  }

  /* Luma */
  if ( Y2_.prediction_mode() == B_PRED ) {
    /* Prediction and inverse transform done in line! */
    Y_.forall_ij( [&] ( const YBlock & block, const unsigned int column, const unsigned int row ) {
	raster.Y_sub.at( column, row ).intra_predict( block.prediction_mode() );
	if ( do_idct ) block.idct( raster.Y_sub.at( column, row ) ); } );
  } else {
    raster.Y.intra_predict( Y2_.prediction_mode() );

    if ( do_idct ) {
      /* transfer the Y2 block with WHT first, if necessary */

      if ( Y2_.coded() ) {
	auto Y_mutable = Y_;
	Y2_.walsh_transform( Y_mutable );
	Y_mutable.forall_ij( [&] ( const YBlock & block, const unsigned int column, const unsigned int row )
			     { block.idct( raster.Y_sub.at( column, row ) ); } );
      } else {
	Y_.forall_ij( [&] ( const YBlock & block, const unsigned int column, const unsigned int row )
		      { block.idct( raster.Y_sub.at( column, row ) ); } );
      }
    }
  }
}

template <class FrameHeaderType, class MacroblockHeaderType>
void Macroblock<FrameHeaderType, MacroblockHeaderType>::loopfilter( const DecoderState & decoder_state,
								    const FilterParameters & frame_loopfilter,
								    const SafeArray< FilterParameters, num_segments > & segment_loopfilters,
								    Raster::Macroblock & raster ) const
{
  const bool skip_subblock_edges = ( Y2_.prediction_mode() != B_PRED ) and ( not has_nonzero_ );

  /* which filter are we using? */
  FilterParameters filter_parameters( header_.segment_id.initialized()
				      ? segment_loopfilters.at( header_.segment_id.get() )
				      : frame_loopfilter );

  filter_parameters.adjust( decoder_state.loopfilter_ref_adjustments,
			    decoder_state.loopfilter_mode_adjustments,
			    CURRENT_FRAME,
			    Y2_.prediction_mode() );

  /* is filter disabled? */
  if ( filter_parameters.filter_level <= 0 ) {
    return;
  }

  switch ( filter_parameters.type ) {
  case LoopFilterType::Normal:
    {
      NormalLoopFilter filter( true, filter_parameters );
      filter.filter( raster, skip_subblock_edges );
    }
    break;
  case LoopFilterType::Simple:
    {
      SimpleLoopFilter filter( filter_parameters );
      filter.filter( raster, skip_subblock_edges );
    }
    break;
  default:
    assert( false );
  }
}

template class Macroblock< KeyFrameHeader, KeyFrameMacroblockHeader >;
template class Macroblock< InterFrameHeader, InterFrameMacroblockHeader >;
