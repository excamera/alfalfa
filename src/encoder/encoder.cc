#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "bool_encoder.hh"
#include "exception.hh"
#include "scorer.hh"
#include "tokens.hh"
#include "decoder_state.hh"

#include "encode_tree.cc"

using namespace std;

static vector< uint8_t > make_frame( const bool key_frame,
				     const bool show_frame,
				     const bool experimental,
				     const uint16_t width,
				     const uint16_t height,
				     const vector< uint8_t > & first_partition,
				     const vector< vector< uint8_t > > & dct_partitions )
{
  if ( width > 16383 or height > 16383 ) {
    throw Invalid( "VP8 frame dimensions too large." );
  }

  if ( dct_partitions.empty() ) {
    throw Invalid( "at least one DCT partition is required." );
  }

  vector< uint8_t > ret;

  const uint32_t first_partition_length = first_partition.size();

  /* frame tag */
  ret.emplace_back( (!key_frame) | (experimental << 3) | (show_frame << 4) | (first_partition_length & 0x7) << 5 );
  ret.emplace_back( (first_partition_length & 0x7f8) >> 3 );
  ret.emplace_back( (first_partition_length & 0x7f800) >> 11 );

  if ( key_frame ) {
    /* start code */
    ret.emplace_back( 0x9d );
    ret.emplace_back( 0x01 );
    ret.emplace_back( 0x2a );

    /* width */
    ret.emplace_back( width & 0xff );
    ret.emplace_back( (width & 0x3f00) >> 8 );

    /* height */
    ret.emplace_back( height & 0xff );
    ret.emplace_back( (height & 0x3f00) >> 8 );
  }

  /* first partition */
  ret.insert( ret.end(), first_partition.begin(), first_partition.end() );

  /* DCT partition lengths */
  for ( unsigned int i = 0; i < dct_partitions.size() - 1; i++ ) {
    const uint32_t length = dct_partitions.at( i ).size();
    ret.emplace_back( length & 0xff );
    ret.emplace_back( (length & 0xff00) >> 8 );
    ret.emplace_back( (length & 0xff0000) >> 16 );
  }

  for ( const auto & dct_partition : dct_partitions ) {
    ret.insert( ret.end(), dct_partition.begin(), dct_partition.end() );
  }

  return ret;
}

template <>
vector< uint8_t > KeyFrame::serialize( const ProbabilityTables & probability_tables ) const
{
  ProbabilityTables frame_probability_tables( probability_tables );
  frame_probability_tables.coeff_prob_update( header() );

  return make_frame( true,
		     show_,
		     continuation_header_.initialized(),
		     display_width_, display_height_,
		     serialize_first_partition( frame_probability_tables ),
		     serialize_tokens( frame_probability_tables ) );
}

template <>
vector< uint8_t > InterFrame::serialize( const ProbabilityTables & probability_tables ) const
{
  ProbabilityTables frame_probability_tables( probability_tables );
  frame_probability_tables.update( header() );

  return make_frame( false,
		     show_,
		     continuation_header_.initialized(),
		     display_width_, display_height_,
		     serialize_first_partition( frame_probability_tables ),
		     serialize_tokens( frame_probability_tables ) );
}

static void encode( BoolEncoder & encoder, const Boolean & flag, const Probability probability = 128 )
{
  encoder.put( flag, probability );
}

template <class T, typename... Targs>
static void encode( BoolEncoder & encoder, const Optional<T> & obj, Targs&&... Fargs )
{
  if ( obj.initialized() ) {
    encode( encoder, obj.get(), forward<Targs>( Fargs )... );
  }
}

template <class T>
static void encode( BoolEncoder & encoder, const Flagged<T> & obj, const Probability probability = 128 )
{
  encoder.put( obj.initialized(), probability );

  if ( obj.initialized() ) {
    encode( encoder, obj.get() );
  }
}

template <unsigned int width>
static void encode( BoolEncoder & encoder, const Unsigned<width> & num )
{
  encoder.put( num & (1 << (width-1)) );
  encode( encoder, Unsigned<width-1>( num ) );
}

static void encode( BoolEncoder &, const Unsigned<0> & ) {}

template <unsigned int width>
static void encode( BoolEncoder & encoder, const Signed<width> & num )
{
  Unsigned<width> absolute_value = abs( num );
  encode( encoder, absolute_value );
  encoder.put( num < 0 );
}

template <class T, unsigned int len>
static void encode( BoolEncoder & encoder, const Array<T, len> & obj )
{
  for ( unsigned int i = 0; i < len; i++ ) {
    encode( encoder, obj.at( i ) );
  }
}

static void encode( BoolEncoder & encoder, const TokenProbUpdate & tpu,
		    const unsigned int l, const unsigned int k,
		    const unsigned int j, const unsigned int i )
{
  encode( encoder, tpu.coeff_prob, k_coeff_entropy_update_probs.at( i ).at( j ).at( k ).at( l ) );
}

static void encode( BoolEncoder & encoder, const MVProbUpdate & mv,
		    const unsigned int j, const unsigned int i )
{
  encode( encoder, mv.mv_prob, k_mv_entropy_update_probs.at( i ).at( j ) );
}

template <class T, unsigned int len, typename... Targs>
static void encode( BoolEncoder & encoder, const Enumerate<T, len> & obj, Targs&&... Fargs )
{
  for ( unsigned int i = 0; i < len; i++ ) {
    encode( encoder, obj.at( i ), i, forward<Targs>( Fargs )... );
  }
}

static void encode( BoolEncoder & encoder, const QuantIndices & h )
{
  encode( encoder, h.y_ac_qi );
  encode( encoder, h.y_dc );
  encode( encoder, h.y2_dc );
  encode( encoder, h.y2_ac );
  encode( encoder, h.uv_dc );
  encode( encoder, h.uv_ac );
}

static void encode( BoolEncoder & encoder, const ModeRefLFDeltaUpdate & h )
{
  encode( encoder, h.ref_update );
  encode( encoder, h.mode_update );
}

static void encode( BoolEncoder & encoder, const SegmentFeatureData & h )
{
  encode( encoder, h.segment_feature_mode );
  encode( encoder, h.quantizer_update );
  encode( encoder, h.loop_filter_update );
}

static void encode( BoolEncoder & encoder, const UpdateSegmentation & h )
{
  encode( encoder, h.update_mb_segmentation_map );
  encode( encoder, h.segment_feature_data );
  encode( encoder, h.mb_segmentation_map );
}

static void encode( BoolEncoder & encoder, const ContinuationHeader & h )
{
  encode( encoder, h.missing_last_frame );
  encode( encoder, h.missing_golden_frame );
  encode( encoder, h.missing_alternate_reference_frame );
}

static void encode( BoolEncoder & encoder, const KeyFrameHeader & header )
{
  encode( encoder, header.color_space );
  encode( encoder, header.clamping_type );
  encode( encoder, header.update_segmentation );
  encode( encoder, header.filter_type );
  encode( encoder, header.loop_filter_level );
  encode( encoder, header.sharpness_level );
  encode( encoder, header.mode_lf_adjustments );
  encode( encoder, header.log2_number_of_dct_partitions );
  encode( encoder, header.quant_indices );
  encode( encoder, header.refresh_entropy_probs );
  encode( encoder, header.token_prob_update );
  encode( encoder, header.prob_skip_false );
}

static void encode( BoolEncoder & encoder, const InterFrameHeader & header )
{
  encode( encoder, header.update_segmentation );
  encode( encoder, header.filter_type );
  encode( encoder, header.loop_filter_level );
  encode( encoder, header.sharpness_level );
  encode( encoder, header.mode_lf_adjustments );
  encode( encoder, header.log2_number_of_dct_partitions );
  encode( encoder, header.quant_indices );
  encode( encoder, header.refresh_golden_frame );
  encode( encoder, header.refresh_alternate_frame );
  encode( encoder, header.copy_buffer_to_golden );
  encode( encoder, header.copy_buffer_to_alternate );
  encode( encoder, header.sign_bias_golden );
  encode( encoder, header.sign_bias_alternate );
  encode( encoder, header.refresh_entropy_probs );
  encode( encoder, header.refresh_last );
  encode( encoder, header.token_prob_update );
  encode( encoder, header.prob_skip_false );
  encode( encoder, header.prob_inter );
  encode( encoder, header.prob_references_last );
  encode( encoder, header.prob_references_golden );
  encode( encoder, header.intra_16x16_prob );
  encode( encoder, header.intra_chroma_prob );
  encode( encoder, header.mv_prob_update );
}

static void encode( BoolEncoder &,
		    const KeyFrameMacroblockHeader &,
		    const KeyFrameHeader & ) {}

static void encode( BoolEncoder & encoder,
		    const InterFrameMacroblockHeader & header,
		    const InterFrameHeader & frame_header )
{
  encode( encoder, header.is_inter_mb, frame_header.prob_inter );
  encode( encoder, header.mb_ref_frame_sel1, frame_header.prob_references_last );
  encode( encoder, header.mb_ref_frame_sel2, frame_header.prob_references_golden );
}

static void encode( BoolEncoder & encoder,
		    const int16_t num,
		    const SafeArray< Probability, MV_PROB_CNT > & component_probs )
{
  enum { IS_SHORT, SIGN, SHORT, BITS = SHORT + 8 - 1, LONG_WIDTH = 10 };

  int16_t num_to_encode = num >> 1;
  uint16_t x = abs( num_to_encode );

  if ( x < 8 ) {
    encode( encoder, false, component_probs.at( IS_SHORT ) );
    const ProbabilityArray< 8 > small_mv_probs = {{ component_probs.at( SHORT ),
						    component_probs.at( SHORT + 1 ),
						    component_probs.at( SHORT + 2 ),
						    component_probs.at( SHORT + 3 ),
						    component_probs.at( SHORT + 4 ),
						    component_probs.at( SHORT + 5 ),
						    component_probs.at( SHORT + 6 ) }};

    /* for unknown reasons, using slice here breaks strict aliasing, but is allowed in macroblock.cc */

    encode( encoder, Tree< int16_t, 8, small_mv_tree >( x ), small_mv_probs );
  } else {
    encode( encoder, true, component_probs.at( IS_SHORT ) );

    for ( uint8_t i = 0; i < 3; i++ ) {
      encode( encoder, (x >> i) & 1, component_probs.at( BITS + i ) );
    }

    for ( uint8_t i = LONG_WIDTH - 1; i > 3; i-- ) {
      encode( encoder, (x >> i) & 1, component_probs.at( BITS + i ) );
    }

    if ( x & 0xfff0 ) {
      encode( encoder, (x >> 3) & 1, component_probs.at( BITS + 3 ) );
    }
  }

  if ( x ) {
    encode( encoder, num_to_encode < 0, component_probs.at( SIGN ) );
  }
}

static void encode( BoolEncoder & encoder,
		    const MotionVector & mv,
		    const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs )
{
  encode( encoder, mv.y(), motion_vector_probs.at( 0 ) );
  encode( encoder, mv.x(), motion_vector_probs.at( 1 ) );
}

template <>
void YBlock::write_subblock_inter_prediction( BoolEncoder & encoder,
					      const MotionVector & best_mv,
					      const SafeArray< SafeArray< Probability, MV_PROB_CNT >, 2 > & motion_vector_probs ) const
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

  encode( encoder,
	  Tree< bmode, num_inter_b_modes, submv_ref_tree >( prediction_mode_ ),
	  submv_ref_probs2.at( submv_ref_index ) );

  if ( prediction_mode_ == NEW4X4 ) {
    MotionVector the_mv( motion_vector_ );
    the_mv -= best_mv;
    encode( encoder, the_mv, motion_vector_probs );
  }
}

template <>
void KeyFrameMacroblock::encode_prediction_modes( BoolEncoder & encoder,
						  const ProbabilityTables & ) const
{
  encode( encoder,
	  Tree< mbmode, num_y_modes, kf_y_mode_tree >( Y2_.prediction_mode() ),
	  kf_y_mode_probs );

  if ( Y2_.prediction_mode() == B_PRED ) {
    Y_.forall( [&]( const YBlock & block ) {
	const auto above_mode = block.context().above.initialized()
	  ? block.context().above.get()->prediction_mode() : B_DC_PRED;
	const auto left_mode = block.context().left.initialized()
	  ? block.context().left.get()->prediction_mode() : B_DC_PRED;
	encode( encoder,
		Tree< bmode, num_intra_b_modes, b_mode_tree >( block.prediction_mode() ),
		kf_b_mode_probs.at( above_mode ).at( left_mode ) );
      } );
  }

  encode( encoder,
	  Tree< mbmode, num_uv_modes, uv_mode_tree >( uv_prediction_mode() ),
	  kf_uv_mode_probs );
}

template <>
void InterFrameMacroblock::encode_prediction_modes( BoolEncoder & encoder,
						    const ProbabilityTables & probability_tables ) const
{
  if ( not inter_coded() ) {
    encode( encoder,
	    Tree< mbmode, num_y_modes, y_mode_tree >( Y2_.prediction_mode() ),
	    probability_tables.y_mode_probs );

    if ( Y2_.prediction_mode() == B_PRED ) {
      Y_.forall( [&]( const YBlock & block ) {
	  encode( encoder,
		  Tree< bmode, num_intra_b_modes, b_mode_tree >( block.prediction_mode() ),
		  invariant_b_mode_probs );
	} );
    }

    encode( encoder,
	    Tree< mbmode, num_uv_modes, uv_mode_tree >( uv_prediction_mode() ),
	    probability_tables.uv_mode_probs );
  } else {
    /* motion-vector "census" */
    Scorer census( header_.motion_vectors_flipped_ );
    census.add( 2, context_.above );
    census.add( 2, context_.left );
    census.add( 1, context_.above_left );
    census.calculate();

    const auto counts = census.mode_contexts();

    /* census determines lookups into fixed probability table */
    const ProbabilityArray< num_mv_refs > mv_ref_probs = {{ mv_counts_to_probs.at( counts.at( 0 ) ).at( 0 ),
							    mv_counts_to_probs.at( counts.at( 1 ) ).at( 1 ),
							    mv_counts_to_probs.at( counts.at( 2 ) ).at( 2 ),
							    mv_counts_to_probs.at( counts.at( 3 ) ).at( 3 ) }};

    encode( encoder,
	    Tree< mbmode, num_mv_refs, mv_ref_tree >( Y2_.prediction_mode() ),
	    mv_ref_probs );

    if ( Y2_.prediction_mode() == NEWMV ) {
      MotionVector the_mv( base_motion_vector() );
      the_mv -= Scorer::clamp( census.best(), context_ );
      encode( encoder, the_mv, probability_tables.motion_vector_probs );
    } else if ( Y2_.prediction_mode() == SPLITMV ) {
      encode( encoder, header_.partition_id.get(), split_mv_probs );
      const auto & partition_scheme = mv_partitions.at( header_.partition_id.get() );

      for ( const auto & this_partition : partition_scheme ) {
	const YBlock & first_subblock = Y_.at( this_partition.front().first,
					       this_partition.front().second );

	first_subblock.write_subblock_inter_prediction( encoder,
							Scorer::clamp( census.best(), context_ ),
							probability_tables.motion_vector_probs );
      }
    }
  }
}

template <class FrameHeaderType, class MacroblockheaderType >
void Macroblock< FrameHeaderType, MacroblockheaderType >::serialize( BoolEncoder & encoder,
								     const FrameHeaderType & frame_header,
								     const ProbabilityArray< num_segments > & mb_segment_tree_probs,
								     const ProbabilityTables & probability_tables ) const
{
  encode( encoder, segment_id_update_, mb_segment_tree_probs );
  encode( encoder, mb_skip_coeff_, frame_header.prob_skip_false.get_or( 0 ) );
  encode( encoder, header_, frame_header );
  encode_prediction_modes( encoder, probability_tables );
}

template <class FrameHeaderType, class MacroblockType>
vector< uint8_t > Frame< FrameHeaderType, MacroblockType >::serialize_first_partition( const ProbabilityTables & probability_tables ) const
{
  BoolEncoder encoder;

  /* encode frame header */
  encode( encoder, header() );

  /* encode continuation header if present */
  encode( encoder, continuation_header_ );

  const auto segment_tree_probs = calculate_mb_segment_tree_probs();

  /* encode the macroblock headers */
  macroblock_headers_.get().forall( [&]( const MacroblockType & macroblock )
				    { macroblock.serialize( encoder,
							    header(),
							    segment_tree_probs,
							    probability_tables ); } );

  return encoder.finish();
}

template <class FrameHeaderType, class MacroblockType>
vector< vector< uint8_t > > Frame< FrameHeaderType, MacroblockType >::serialize_tokens( const ProbabilityTables & probability_tables ) const
{
  vector< BoolEncoder > dct_partitions( dct_partition_count() );

  /* serialize every macroblock's tokens */
  macroblock_headers_.get().forall_ij( [&]( const MacroblockType & macroblock,
					    const unsigned int column __attribute((unused)),
					    const unsigned int row )
				       {
					 const bool continuation_mb = continuation_header_.initialized()
					   and macroblock.inter_coded()
					   and continuation_header_.get().is_missing( macroblock.reference() );

					 macroblock.serialize_tokens( dct_partitions.at( row % dct_partition_count() ),
								      probability_tables,
								      continuation_mb ); } );

  /* finish encoding and return the resulting octet sequences */
  vector< vector< uint8_t > > ret;
  for ( auto & x : dct_partitions ) {
    ret.emplace_back( x.finish() );
  }
  return ret;
}

template <class FrameHeaderType, class MacroblockheaderType >
void Macroblock< FrameHeaderType, MacroblockheaderType >::serialize_tokens( BoolEncoder & encoder,
									    const ProbabilityTables & probability_tables,
									    const bool continuation ) const
{
  if ( mb_skip_coeff_.get_or( false ) ) {
    return;
  }

  if ( Y2_.coded() ) {
    Y2_.serialize_tokens( encoder, probability_tables, false );
  }

  Y_.forall( [&]( const YBlock & block ) { block.serialize_tokens( encoder, probability_tables, continuation ); } );
  U_.forall( [&]( const UVBlock & block ) { block.serialize_tokens( encoder, probability_tables, continuation ); } );
  V_.forall( [&]( const UVBlock & block ) { block.serialize_tokens( encoder, probability_tables, continuation ); } );
}

template <unsigned int length>
void TokenDecoder<length>::encode( BoolEncoder & encoder, const uint16_t value ) const
{
  assert( value >= base_value_ );
  uint16_t increment = value - base_value_;
  for ( uint8_t i = 0; i < length; i++ ) {
    encoder.put( increment & (1 << (length - 1 - i)), bit_probabilities_.at( i ) );
  }
}

template <BlockType initial_block_type, class PredictionMode>
void Block< initial_block_type,
	    PredictionMode >::serialize_tokens( BoolEncoder & encoder,
						const ProbabilityTables & probability_tables,
						const bool continuation ) const
{
  uint8_t coded_length = 0;

  /* how many tokens are we going to encode? */
  for ( unsigned int index = (type_ == BlockType::Y_after_Y2) ? 1 : 0;
	index < 16;
	index++ ) {
    if ( coefficients_.at( zigzag.at( index ) ) ) {
      coded_length = index + 1;
    }
  }

  bool last_was_zero = false;

  /* prediction context starts with number-not-zero count */
  char token_context = ( context().above.initialized() ? context().above.get()->has_nonzero() : 0 )
    + ( context().left.initialized() ? context().left.get()->has_nonzero() : 0 );

  unsigned int index = (type_ == BlockType::Y_after_Y2) ? 1 : 0;

  for ( ; index < coded_length; index++ ) {
    const int16_t coefficient = coefficients_.at( zigzag.at( index ) );
    const uint16_t token_value = abs( coefficient );
    const bool coefficient_sign = coefficient < 0;

    /* select the tree probabilities based on the prediction context */
    const ProbabilityArray< MAX_ENTROPY_TOKENS > & prob
      = probability_tables.coeff_probs.at( type_ ).at( coefficient_to_band.at( index ) ).at( token_context );

    if ( not last_was_zero ) {
      encoder.put( true, prob.at( 0 ) );
    }

    if ( token_value == 0 ) {
      encoder.put( false, prob.at( 1 ) );
      last_was_zero = true;
      token_context = 0;
      continue;
    }

    last_was_zero = false;
    encoder.put( true, prob.at( 1 ) );

    if ( token_value == 1 ) {
      encoder.put( false, prob.at( 2 ) );
      encoder.put( coefficient_sign );
      token_context = 1;
      continue;
    }

    token_context = 2;
    encoder.put( true, prob.at( 2 ) );

    if ( token_value == 2 ) {
      encoder.put( false, prob.at( 3 ) );
      encoder.put( false, prob.at( 4 ) );
      encoder.put( coefficient_sign );
      continue;
    }

    if ( token_value == 3 ) {
      encoder.put( false, prob.at( 3 ) );
      encoder.put( true, prob.at( 4 ) );
      encoder.put( false, prob.at( 5 ) );
      encoder.put( coefficient_sign );
      continue;
    }

    if ( token_value == 4 ) {
      encoder.put( false, prob.at( 3 ) );
      encoder.put( true, prob.at( 4 ) );
      encoder.put( true, prob.at( 5 ) );
      encoder.put( coefficient_sign );
      continue;
    }

    encoder.put( true, prob.at( 3 ) );

    if ( token_value < token_decoder_1.base_value() ) { /* category 1, 5..6 */
      encoder.put( false, prob.at( 6 ) );
      encoder.put( false, prob.at( 7 ) );
      encoder.put( token_value == 6, 159 );
      encoder.put( coefficient_sign );
      continue;
    }

    if ( token_value < token_decoder_1.upper_limit() ) { /* category 2, 7..10 */
      encoder.put( false, prob.at( 6 ) );
      encoder.put( true, prob.at( 7 ) );
      token_decoder_1.encode( encoder, token_value );
      encoder.put( coefficient_sign );
      continue;
    }

    encoder.put( true, prob.at( 6 ) );

    if ( token_value < token_decoder_2.upper_limit() ) { /* category 3, 11..18 */
      encoder.put( false, prob.at( 8 ) );
      encoder.put( false, prob.at( 9 ) );
      token_decoder_2.encode( encoder, token_value );
      encoder.put( coefficient_sign );
      continue;
    }

    if ( token_value < token_decoder_3.upper_limit() ) { /* category 4, 19..34 */
      encoder.put( false, prob.at( 8 ) );
      encoder.put( true, prob.at( 9 ) );
      token_decoder_3.encode( encoder, token_value );
      encoder.put( coefficient_sign );
      continue;
    }

    encoder.put( true, prob.at( 8 ) );

    if ( token_value < token_decoder_4.upper_limit() ) { /* category 5, 35..66 */
      encoder.put( false, prob.at( 10 ) );
      token_decoder_4.encode( encoder, token_value );
      encoder.put( coefficient_sign );
      continue;
    }

    if ( token_value < token_decoder_5.upper_limit() ) { /* category 6, 67..2048 */
      encoder.put( true, prob.at( 10 ) );
      token_decoder_5.encode( encoder, token_value );
      encoder.put( coefficient_sign );
      continue;
    }

    throw Exception( "token encoder", "value too large" );
  }

  assert( last_was_zero == false );

  /* write end of block */
  if ( coded_length < 16 ) {
    const ProbabilityArray< MAX_ENTROPY_TOKENS > & prob
      = probability_tables.coeff_probs.at( type_ ).at( coefficient_to_band.at( index ) ).at( token_context );

    encoder.put( false, prob.at( 0 ) );
  }

  /* write pixel adjustment */
  if ( continuation ) {
    for ( unsigned int i = 0; i < 16; i++ ) {
      const bool is_nonzero = !( pixel_adjustments_.at( i ) == PixelAdjustment::NoAdjustment );
      encoder.put( is_nonzero, 253 );

      if ( is_nonzero ) {
	const bool bit = (pixel_adjustments_.at( i ) == PixelAdjustment::PlusOne) ? 1 : 0;
	encoder.put( bit );
      }
    }
  }
}
