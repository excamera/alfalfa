#include <vector>
#include <algorithm>

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
  : context_( c ),
    header_( data, frame_header, decoder_state ),
    Y2_( frame_Y2.at( c.column, c.row ) ),
    Y_( frame_Y, c.column * 4, c.row * 4 ),
    U_( frame_U, c.column * 2, c.row * 2 ),
    V_( frame_V, c.column * 2, c.row * 2 )
{
  decode_prediction_modes( data, decoder_state, frame_header );
}

template <>
void KeyFrameMacroblock::decode_prediction_modes( BoolDecoder & data,
						  const DecoderState &,
						  const KeyFrameHeader & )
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
void InterFrameMacroblock::set_base_motion_vector( const MotionVector & mv )
{
  Y_.at( 3, 3 ).set_motion_vector( mv );
}

template <>
const MotionVector & InterFrameMacroblock::base_motion_vector( void ) const
{
  return Y_.at( 3, 3 ).motion_vector();
}

template <>
bool KeyFrameMacroblock::inter_coded( void ) const
{
  return false;
}

template <>
bool InterFrameMacroblock::inter_coded( void ) const
{
  return header_.is_inter_mb;
}

class Scorer
{
private:
  bool motion_vectors_flipped_;

  SafeArray< uint8_t, 4 > scores_ {{}};
  SafeArray< MotionVector, 4 > motion_vectors_ {{}};

  uint8_t splitmv_score_ {};

  uint8_t index_ {};

  void add( const uint8_t score, const MotionVector & mv )
  {
    if ( mv.empty() ) {
      scores_.at( 0 ) += score;
    } else {
      if ( not ( mv == motion_vectors_.at( index_ ) ) ) {
	index_++;
	motion_vectors_.at( index_ ) = mv;
      }

      scores_.at( index_ ) += score;
    }
  }

public:
  Scorer( const bool motion_vectors_flipped ) : motion_vectors_flipped_( motion_vectors_flipped ) {}

  const MotionVector & best( void ) const { return motion_vectors_.at( 0 ); }
  const MotionVector & nearest( void ) const { return motion_vectors_.at( 1 ); }
  const MotionVector & near( void ) const { return motion_vectors_.at( 2 ); }

  void add( const uint8_t score, const Optional< const InterFrameMacroblock * > & mb )
  {
    if ( mb.initialized() and mb.get()->inter_coded() ) {
      MotionVector mv = mb.get()->base_motion_vector();
      if ( mb.get()->header().motion_vectors_flipped_ != motion_vectors_flipped_ ) {
	mv = -mv;
      }
      add( score, mv );
      if ( mb.get()->y_prediction_mode() == SPLITMV ) {
	splitmv_score_ += score;
      }
    }
  }

  void calculate( void )
  {
    if ( scores_.at( 3 ) ) {
      if ( motion_vectors_.at( index_ ) == motion_vectors_.at( 1 ) ) {
	scores_.at( 1 ) += scores_.at( 3 );
      }
    }

    if ( scores_.at( 2 ) > scores_.at( 1 ) ) {
      tie( scores_.at( 1 ), scores_.at( 2 ) ) = make_pair( scores_.at( 2 ), scores_.at( 1 ) );
      tie( motion_vectors_.at( 1 ), motion_vectors_.at( 2 ) )
	= make_pair( motion_vectors_.at( 2 ), motion_vectors_.at( 1 ) );
    }

    if ( scores_.at( 1 ) >= scores_.at( 0 ) ) {
      motion_vectors_.at( 0 ) = motion_vectors_.at( 1 );
    }
  }

  SafeArray< uint8_t, 4 > mode_contexts( void ) const
  {
    return { scores_.at( 0 ), scores_.at( 1 ), scores_.at( 2 ), splitmv_score_ };
  }
};

void MotionVector::clamp( const int16_t to_left, const int16_t to_right,
			  const int16_t to_top, const int16_t to_bottom )
{
  x_ = min( max( x_, to_left ), to_right );
  y_ = min( max( y_, to_top ), to_bottom );
}

MotionVector clamp( const MotionVector & mv, const typename TwoD< InterFrameMacroblock >::Context & c )
{
  MotionVector ret( mv );

  const int16_t to_left = -((c.column * 16) << 3) - 128;
  const int16_t to_right = (((c.width - 1 - c.column) * 16) << 3) + 128;
  const int16_t to_top = (-((c.row * 16)) << 3) - 128;
  const int16_t to_bottom = (((c.height - 1 - c.row) * 16) << 3) + 128;

  ret.clamp( to_left, to_right, to_top, to_bottom );

  return ret;
}

/* Taken from libvpx dixie read_mv_component() */
int16_t MotionVector::read_component( BoolDecoder & data,
				      const SafeArray< Probability, MV_PROB_CNT > & component_probs )
{
  enum { IS_SHORT, SIGN, SHORT, BITS = SHORT + 8 - 1, LONG_WIDTH = 10 };
  int16_t x = 0;

  if ( data.get( component_probs.at( IS_SHORT ) ) ) { /* Large */
    for ( uint8_t i = 0; i < 3; i++ ) {
      x += data.get( component_probs.at( BITS + i ) ) << i;
    }

    /* Skip bit 3, which is sometimes implicit */
    for ( uint8_t i = LONG_WIDTH - 1; i > 3; i-- ) {
      x += data.get( component_probs.at( BITS + i ) ) << i;
    }

    if ( !(x & 0xFFF0) or data.get( component_probs.at( BITS + 3 ) ) ) {
      x += 8;
    }
  } else {  /* small */
    const ProbabilityArray< 8 > & small_mv_probs = component_probs.slice<SHORT, 7>();

    x = data.tree< 8, uint8_t >( small_mv_tree, small_mv_probs );
  }

  if ( x && data.get( component_probs.at( SIGN ) ) ) {
    x = -x;
  }

  return x << 1;
}

template <>
void YBlock::read_subblock_inter_prediction( BoolDecoder & data,
					     const MotionVector & best_mv,
					     const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs )
{
  const MotionVector default_mv;

  const MotionVector & left_mv = context().left.initialized() ? context().left.get()->motion_vector() : default_mv;

  const MotionVector & above_mv = context().above.initialized() ? context().above.get()->motion_vector() : default_mv;

  const bool left_is_zero = left_mv.empty();
  const bool above_is_zero = above_mv.empty();
  const bool left_eq_above = left_mv == above_mv;

  uint8_t submv_ref_index = 0;

  if ( left_eq_above and left_is_zero ) {
    submv_ref_index = 4;
  } else if ( left_eq_above ) {
    submv_ref_index = 3;
  } else if ( above_is_zero ) {
    submv_ref_index = 2;
  } else if ( left_is_zero ) {
    submv_ref_index = 1;
  }

  prediction_mode_ = data.tree< num_inter_b_modes, bmode >( submv_ref_tree,
							    submv_ref_probs2.at( submv_ref_index ) );

  switch ( prediction_mode_ ) {
  case LEFT4X4:
    motion_vector_ = left_mv;
    break;
  case ABOVE4X4:
    motion_vector_ = above_mv;
    break;
  case ZERO4X4: /* zero by default */
    assert( motion_vector_.empty() );
    break;
  case NEW4X4:
    {
      MotionVector new_mv( data, motion_vector_probs );
      new_mv += best_mv;
      motion_vector_ = new_mv;
    }
    break;
  default:
    assert( false );
    break;
  }
}

MotionVector::MotionVector( BoolDecoder & data,
			    const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs )
  : y_( read_component( data, motion_vector_probs.at( 0 ) ) ),
    x_( read_component( data, motion_vector_probs.at( 1 ) ) )
{}

MotionVector luma_to_chroma( const MotionVector & s1,
			     const MotionVector & s2,
			     const MotionVector & s3,
			     const MotionVector & s4 )
{
  const int16_t x = s1.x() + s2.x() + s3.x() + s4.x();
  const int16_t y = s1.y() + s2.y() + s3.y() + s4.y();

  return MotionVector( x >= 0 ?  (x + 4) >> 3 : -((-x + 4) >> 3),
		       y >= 0 ?  (y + 4) >> 3 : -((-y + 4) >> 3) );
}

template <>
void InterFrameMacroblock::decode_prediction_modes( BoolDecoder & data,
						    const DecoderState & decoder_state,
						    const InterFrameHeader &  )
{
  if ( not inter_coded() ) {
    /* Set Y prediction mode */
    Y2_.set_prediction_mode( data.tree< num_y_modes, mbmode >( y_mode_tree, decoder_state.y_mode_probs ) );
    Y2_.set_if_coded();

    /* Set subblock prediction modes. Intra macroblocks in interframes are simpler than in keyframes. */
    Y_.forall( [&]( YBlock & block )
	       {
		 if ( Y2_.prediction_mode() == B_PRED ) {
		   block.set_Y_without_Y2();
		   block.set_prediction_mode( data.tree< num_intra_b_modes, bmode >( b_mode_tree,
										     invariant_b_mode_probs ) );
		 } else {
		   block.set_prediction_mode( implied_subblock_mode( Y2_.prediction_mode() ) );
		 }
	       } );

    /* Set chroma prediction modes */
    U_.at( 0, 0 ).set_prediction_mode( data.tree< num_uv_modes, mbmode >( uv_mode_tree,
									  decoder_state.uv_mode_probs ) );
  } else {
    /* motion-vector "census" */
    Scorer census( header_.motion_vectors_flipped_ );
    census.add( 2, context_.above );
    census.add( 2, context_.left );
    census.add( 1, context_.above_left );
    census.calculate();

    const auto counts = census.mode_contexts();

    /* census determines lookups into fixed probability table */
    const ProbabilityArray< 5 > mv_ref_probs = {{ mv_counts_to_probs.at( counts.at( 0 ) ).at( 0 ),
						  mv_counts_to_probs.at( counts.at( 1 ) ).at( 1 ),
						  mv_counts_to_probs.at( counts.at( 2 ) ).at( 2 ),
						  mv_counts_to_probs.at( counts.at( 3 ) ).at( 3 ) }};

    Y2_.set_prediction_mode( data.tree< 5, mbmode >( mv_ref_tree, mv_ref_probs ) );
    Y2_.set_if_coded();

    switch ( Y2_.prediction_mode() ) {
    case NEARESTMV:
      set_base_motion_vector( clamp( census.nearest(), context_ ) );
      break;
    case NEARMV:
      set_base_motion_vector( clamp( census.near(), context_ ) );
      break;
    case ZEROMV:
      set_base_motion_vector( MotionVector() );
      break;
    case NEWMV:
      {
	MotionVector new_mv( data, decoder_state.motion_vector_probs );
	new_mv += clamp( census.best(), context_ );
	set_base_motion_vector( new_mv );
      }
      break;
    case SPLITMV:
      {
	const uint8_t partition_id = data.tree< 4, uint8_t >( split_mv_tree, split_mv_probs );
	const auto & partition_scheme = mv_partitions.at( partition_id );

	for ( const auto & this_partition : partition_scheme ) {
	  YBlock & first_subblock = Y_.at( this_partition.front().first,
					   this_partition.front().second );

	  first_subblock.read_subblock_inter_prediction( data,
							 clamp( census.best(), context_ ),
							 decoder_state.motion_vector_probs );	  

	  /* copy to rest of subblocks */

	  for ( auto it = this_partition.begin() + 1; it != this_partition.end(); it++ ) {
	    YBlock & other_subblock = Y_.at( it->first, it->second );
	    other_subblock.set_prediction_mode( first_subblock.prediction_mode() );
	    other_subblock.set_motion_vector( first_subblock.motion_vector() );
	  }
	}
      }
      break;
    default:
      assert( false );
      break;
    }

    /* set motion vectors of Y subblocks */
    if ( Y2_.prediction_mode() != SPLITMV ) {
      Y_.forall( [&] ( YBlock & block ) { block.set_motion_vector( base_motion_vector() ); } );
    }

    /* set motion vectors of chroma subblocks */
    U_.forall_ij( [&]( UVBlock & block, const unsigned int column, const unsigned int row ) {
	block.set_motion_vector( luma_to_chroma( Y_.at( column * 2, row * 2 ).motion_vector(),
						 Y_.at( column * 2 + 1, row * 2 ).motion_vector(),
						 Y_.at( column * 2, row * 2 + 1 ).motion_vector(),
						 Y_.at( column * 2 + 1, row * 2 + 1 ).motion_vector() ) );
      } );

    V_.forall_ij( [&]( UVBlock & block, const unsigned int column, const unsigned int row ) {
	block.set_motion_vector( U_.at( column, row ).motion_vector() );
      } );
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
  mb_ref_frame_sel2( mb_ref_frame_sel1.get_or( false ), data, frame_header.prob_references_golden ),
  motion_vectors_flipped_( ( (reference() == GOLDEN_FRAME) and (frame_header.sign_bias_golden) )
			   or ( (reference() == ALTREF_FRAME) and (frame_header.sign_bias_alternate) ) )
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
void Macroblock<FrameHeaderType, MacroblockHeaderType>::predict_and_inverse_transform( Raster::Macroblock & raster ) const
{
  if ( inter_coded() ) {
    return;
  }

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
  const bool skip_subblock_edges = Y2_.coded() and ( not has_nonzero_ );

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

reference_frame InterFrameMacroblockHeader::reference( void ) const
{
  if ( not is_inter_mb ) {
    return CURRENT_FRAME;
  }

  if ( not mb_ref_frame_sel1.get() ) {
    return LAST_FRAME;
  }

  if ( not mb_ref_frame_sel2.get() ) {
    return GOLDEN_FRAME;
  }

  return ALTREF_FRAME;
}

template class Macroblock< KeyFrameHeader, KeyFrameMacroblockHeader >;
template class Macroblock< InterFrameHeader, InterFrameMacroblockHeader >;
