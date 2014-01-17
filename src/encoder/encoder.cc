#include "encoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "bool_encoder.hh"
#include "exception.hh"
#include "scorer.hh"

#include "encode_tree.cc"

using namespace std;

Encoder::Encoder( const uint16_t width, const uint16_t height, const Chunk & key_frame )
  : width_( width ), height_( height ),
    state_( KeyFrame( UncompressedChunk( key_frame, width_, height_ ),
		      width_, height_ ).header(),
	    width_, height_ )
{}

vector< uint8_t > Encoder::encode_frame( const Chunk & frame )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, width_, height_ );

  vector< uint8_t > first_partition;

  if ( uncompressed_chunk.key_frame() ) {
    /* parse keyframe header */
    KeyFrame myframe( uncompressed_chunk, width_, height_ );

    /* reset persistent decoder state to default values */
    state_ = DecoderState( myframe.header(), width_, height_ );

    /* calculate new probability tables. replace persistent copy if prescribed in header */
    ProbabilityTables frame_probability_tables( state_.probability_tables );
    frame_probability_tables.coeff_prob_update( myframe.header() );
    if ( myframe.header().refresh_entropy_probs ) {
      state_.probability_tables = frame_probability_tables;
    }

    /* decode the frame (and update the persistent segmentation map) */
    myframe.parse_macroblock_headers_and_update_segmentation_map( state_.segmentation_map, frame_probability_tables );
    myframe.parse_tokens( state_.quantizer_filter_adjustments, frame_probability_tables );

    first_partition = myframe.serialize_first_partition();
  } else {
    /* parse interframe header */
    InterFrame myframe( uncompressed_chunk, width_, height_ );

    /* update adjustments to quantizer and in-loop deblocking filter */
    state_.quantizer_filter_adjustments.update( myframe.header() );

    /* update probability tables. replace persistent copy if prescribed in header */
    ProbabilityTables frame_probability_tables( state_.probability_tables );
    frame_probability_tables.update( myframe.header() );
    if ( myframe.header().refresh_entropy_probs ) {
      state_.probability_tables = frame_probability_tables;
    }

    /* decode the frame (and update the persistent segmentation map) */
    myframe.parse_macroblock_headers_and_update_segmentation_map( state_.segmentation_map, frame_probability_tables );
    myframe.parse_tokens( state_.quantizer_filter_adjustments, frame_probability_tables );

    first_partition = myframe.serialize_first_partition();
  }

  return first_partition;
}

static void encode( BoolEncoder & encoder, const Boolean & flag, const Probability probability = 128 )
{
  encoder.put( flag, probability );
}

template <class T>
static void encode( BoolEncoder & encoder, const Optional<T> & obj )
{
  if ( obj.initialized() ) {
    encode( encoder, obj.get() );
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

static void encode( BoolEncoder &, const KeyFrameMacroblockHeader & ) {}

static void encode( BoolEncoder & encoder, const InterFrameMacroblockHeader & header )
{
  encode( encoder, header.is_inter_mb );
  encode( encoder, header.mb_ref_frame_sel1 );
  encode( encoder, header.mb_ref_frame_sel2 );
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
  }
}

template <class FrameHeaderType, class MacroblockheaderType >
void Macroblock< FrameHeaderType, MacroblockheaderType >::serialize( BoolEncoder & encoder,
								     const FrameHeaderType & frame_header,
								     const ProbabilityArray< num_segments > & mb_segment_tree_probs ) const
{
  if ( segment_id_update_.initialized() ) {
    encode( encoder, segment_id_update_.get(), mb_segment_tree_probs );
  }

  if ( mb_skip_coeff_.initialized() ) {
    encode( encoder, mb_skip_coeff_.get(), frame_header.prob_skip_false.get_or( 0 ) );
  }

  encode( encoder, header_ );
}

template <class FrameHeaderType, class MacroblockType>
vector< uint8_t > Frame< FrameHeaderType, MacroblockType >::serialize_first_partition( void ) const
{
  BoolEncoder encoder;

  /* encode partition header */
  encode( encoder, header() );

  const auto segment_tree_probs = calculate_mb_segment_tree_probs();

  /* encode the macroblock headers */
  macroblock_headers_.get().forall( [&]( const MacroblockType & macroblock )
				    { macroblock.serialize( encoder, header(), segment_tree_probs ); } );

  return encoder.finish();
}
