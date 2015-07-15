#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder_state.hh"
#include "continuation_state.hh"

#include <sstream>

using namespace std;

Decoder::Decoder( const uint16_t width, const uint16_t height )
  : state_( width, height ),
    references_( width, height ),
    continuation_raster_( references_.last )
{}

Optional<RasterHandle> Decoder::decode_frame( const Chunk & frame )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  /* get a RasterHandle */
  MutableRasterHandle raster { state_.width, state_.height };

  if ( uncompressed_chunk.key_frame() ) {
    const KeyFrame myframe = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    const bool shown = myframe.show_frame();

    myframe.decode( state_.segmentation, raster );

    update_continuation( raster );

    myframe.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    RasterHandle immutable_raster( move( raster ) );

    myframe.copy_to( immutable_raster, references_ );

    return Optional<RasterHandle>( shown, immutable_raster );
  } else {
    const InterFrame myframe = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    const bool shown = myframe.show_frame();

    myframe.decode( state_.segmentation, references_, continuation_raster_, raster );

    update_continuation( raster );

    myframe.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    RasterHandle immutable_raster( move( raster ) );

    myframe.copy_to( immutable_raster, references_ );

    return Optional<RasterHandle>( shown, immutable_raster );
  }
}

ContinuationState Decoder::next_continuation_state( const Chunk & frame )
{
  References prev_references = references_;

  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  /* get a RasterHandle */
  MutableRasterHandle raster { state_.width, state_.height };

  if ( uncompressed_chunk.key_frame() ) {
    KeyFrame myframe = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    bool shown = myframe.show_frame();

    myframe.decode( state_.segmentation, raster );

    update_continuation( raster );

    myframe.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    RasterHandle immutable_raster( move( raster ) );

    myframe.copy_to( immutable_raster, references_ );

    return ContinuationState( move( myframe ), Optional<RasterHandle>( shown, immutable_raster ),
                              prev_references );
  } else {
    InterFrame myframe = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    bool shown = myframe.show_frame();

    myframe.decode( state_.segmentation, references_, continuation_raster_, raster );

    update_continuation( raster );

    myframe.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    RasterHandle immutable_raster( move( raster ) );

    myframe.copy_to( immutable_raster, references_ );

    return ContinuationState( move( myframe ), Optional<RasterHandle>( shown, immutable_raster ),
                              prev_references );
  }
}

DecoderHash Decoder::get_hash( void ) const
{
  return DecoderHash( state_.hash(), continuation_raster_.hash(),
                      references_.last.hash(), references_.golden.hash(),
                      references_.alternative_reference.hash() );
}

void Decoder::sync_continuation_raster( const Decoder & other )
{
  continuation_raster_ = other.continuation_raster_;
}

bool Decoder::operator==( const Decoder & other ) const
{
  return state_ == other.state_ and continuation_raster_ == other.continuation_raster_ and
    references_.last == other.references_.last and references_.golden == other.references_.golden and
    references_.alternative_reference == other.references_.alternative_reference;
}

DecoderDiff Decoder::operator-( const Decoder & other ) const
{
  // FIXME maybe DecoderState should just go in here, since the state calculations we
  // do are useless for keyframes
  return DecoderDiff { RasterDiff( continuation_raster_, other.continuation_raster_ ),
                       other.get_hash(), get_hash(),
                       state_.get_replacement_probs( other.state_ ),
                       state_.probability_tables,
                       state_.get_filter_update(),
                       state_.get_segment_update() };
}

References::References( const uint16_t width, const uint16_t height )
  : References( MutableRasterHandle( width, height ) )
{}

References::References( MutableRasterHandle && raster )
  : last( move( raster ) ),
    golden( last ),
    alternative_reference( last )
{}

void Decoder::update_continuation( const MutableRasterHandle & raster )
{
  MutableRasterHandle copy_raster( raster.get().display_width(), raster.get().display_height() );

  copy_raster.get().copy_from( raster );
  continuation_raster_ = RasterHandle( move( copy_raster ) );
}

DecoderState::DecoderState( const unsigned int s_width, const unsigned int s_height )
  : width( s_width ), height( s_height )
{}

DecoderState::DecoderState( const KeyFrameHeader & header,
			    const unsigned int s_width,
			    const unsigned int s_height )
  : width( s_width ), height( s_height ),
    segmentation( header.update_segmentation.initialized(), header, width, height ),
    filter_adjustments( header.mode_lf_adjustments.initialized(), header )
{}

bool DecoderState::operator==( const DecoderState & other ) const
{
  return width == other.width
    and height == other.height
    and probability_tables == other.probability_tables
    and segmentation == other.segmentation
    and filter_adjustments == other.filter_adjustments;
}

// FIXME These functions should probably go in continuation.cc
ReplacementEntropyHeader DecoderState::get_replacement_probs( const DecoderState & other ) const
{
  ReplacementEntropyHeader replacement_entropy_header;

  /* match (normal) coefficient probabilities in frame header */
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
	for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
	  const auto & source = other.probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l );
	  const auto & target = probability_tables.coeff_probs.at( i ).at( j ).at( k ).at( l );

	  replacement_entropy_header.token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( source != target, target );
	}
      }
    }
  }

  /* match intra_16x16_probs in frame header */
  bool update_y_mode_probs = false;
  Array< Unsigned< 8 >, 4 > new_y_mode_probs;

  for ( unsigned int i = 0; i < 4; i++ ) {
    const auto & source = other.probability_tables.y_mode_probs.at( i );
    const auto & target = probability_tables.y_mode_probs.at( i );

    new_y_mode_probs.at( i ) = target;

    if ( source != target ) {
      update_y_mode_probs = true;
    }
  }

  if ( update_y_mode_probs ) {
    replacement_entropy_header.intra_16x16_prob.initialize( new_y_mode_probs );
  }

  /* match intra_chroma_prob in frame header */
  bool update_chroma_mode_probs = false;
  Array< Unsigned< 8 >, 3 > new_chroma_mode_probs;

  for ( unsigned int i = 0; i < 3; i++ ) {
    const auto & source = other.probability_tables.uv_mode_probs.at( i );
    const auto & target = probability_tables.uv_mode_probs.at( i );

    new_chroma_mode_probs.at( i ) = target;

    if ( source != target ) {
      update_chroma_mode_probs = true;
    }
  }

  if ( update_chroma_mode_probs ) {
    replacement_entropy_header.intra_chroma_prob.initialize( new_chroma_mode_probs );
  }

  /* match motion_vector_probs in frame header */
  for ( uint8_t i = 0; i < 2; i++ ) {
    for ( uint8_t j = 0; j < MV_PROB_CNT; j++ ) {
      const auto & source = other.probability_tables.motion_vector_probs.at( i ).at( j );
      const auto & target = probability_tables.motion_vector_probs.at( i ).at( j );

      replacement_entropy_header.mv_prob_update.at( i ).at( j ) = MVProbReplacement( source != target, target );
    }
  }

  return replacement_entropy_header;
}

Optional<ModeRefLFDeltaUpdate> DecoderState::get_filter_update( void ) const
{
  if ( not filter_adjustments.initialized() ) {
    return Optional<ModeRefLFDeltaUpdate>();
  }
  ModeRefLFDeltaUpdate filter_update;

  /* these are 0 if not set */
  for ( unsigned int i = 0; i < filter_adjustments.get().loopfilter_ref_adjustments.size(); i++ ) {
    const auto & value = filter_adjustments.get().loopfilter_ref_adjustments.at( i );
    if ( value ) {
      filter_update.ref_update.at( i ).initialize( value );
    }
  }

  for ( unsigned int i = 0; i < filter_adjustments.get().loopfilter_mode_adjustments.size(); i++ ) {
    const auto & value = filter_adjustments.get().loopfilter_mode_adjustments.at( i );
    if ( value ) {
      filter_update.mode_update.at( i ).initialize( value );
    }
  }

  return Optional<ModeRefLFDeltaUpdate>( move( filter_update ) );
}

Optional<SegmentFeatureData> DecoderState::get_segment_update( void ) const
{
  if ( not segmentation.initialized() ) {
    return Optional<SegmentFeatureData>();
  }

  SegmentFeatureData segment_feature_data;

  segment_feature_data.segment_feature_mode = segmentation.get().absolute_segment_adjustments;

  for ( unsigned int i = 0; i < num_segments; i++ ) {
    /* these also default to 0 */
    const auto & q_adjustment = segmentation.get().segment_quantizer_adjustments.at( i );
    const auto & lf_adjustment = segmentation.get().segment_filter_adjustments.at( i );
    if ( q_adjustment ) {
      segment_feature_data.quantizer_update.at( i ).initialize( q_adjustment );
    }

    if ( lf_adjustment ) {
      segment_feature_data.loop_filter_update.at( i ).initialize( lf_adjustment );
    }
  }

  return Optional<SegmentFeatureData>( move( segment_feature_data ) );
}

size_t DecoderState::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_combine( hash_val, width );
  boost::hash_combine( hash_val, height );
  boost::hash_combine( hash_val, probability_tables.hash() );
  if ( segmentation.initialized() ) {
    boost::hash_combine( hash_val, segmentation.get().hash() );
  }
  if ( filter_adjustments.initialized() ) {
    boost::hash_combine( hash_val, filter_adjustments.get().hash() );
  }

  return hash_val;
}

size_t ProbabilityTables::hash( void ) const
{
  size_t hash_val = 0;

  for ( auto const & block_sub : coeff_probs ) {
    for ( auto const & bands_sub : block_sub ) {
      for ( auto const & contexts_sub : bands_sub ) {
	boost::hash_range( hash_val, contexts_sub.begin(), contexts_sub.end() );
      }
    }
  }

  boost::hash_range( hash_val, y_mode_probs.begin(), y_mode_probs.end() );

  boost::hash_range( hash_val, uv_mode_probs.begin(), uv_mode_probs.end() );

  for ( auto const & sub : motion_vector_probs ) {
    boost::hash_range( hash_val, sub.begin(), sub.end() );
  }

  return hash_val;
}

size_t FilterAdjustments::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_range( hash_val, loopfilter_ref_adjustments.begin(), loopfilter_ref_adjustments.end() );

  boost::hash_range( hash_val, loopfilter_mode_adjustments.begin(), loopfilter_ref_adjustments.end() );

  return hash_val;
}

size_t Segmentation::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_combine( hash_val, absolute_segment_adjustments );

  boost::hash_range( hash_val, segment_quantizer_adjustments.begin(),
		       segment_quantizer_adjustments.end() );

  boost::hash_range( hash_val, segment_filter_adjustments.begin(),
		       segment_filter_adjustments.end() );

  boost::hash_range( hash_val, map.begin(), map.end() );

  return hash_val;
}

bool ProbabilityTables::operator==( const ProbabilityTables & other ) const
{
  return coeff_probs == other.coeff_probs
    and y_mode_probs == other.y_mode_probs
    and uv_mode_probs == other.uv_mode_probs
    and motion_vector_probs == other.motion_vector_probs;
}

bool FilterAdjustments::operator==( const FilterAdjustments & other ) const
{
  return loopfilter_ref_adjustments == other.loopfilter_ref_adjustments
    and loopfilter_mode_adjustments == other.loopfilter_mode_adjustments;
}

bool Segmentation::operator==( const Segmentation & other ) const
{
  return absolute_segment_adjustments == other.absolute_segment_adjustments
    and segment_quantizer_adjustments == other.segment_quantizer_adjustments
    and segment_filter_adjustments == other.segment_filter_adjustments
    and map == other.map;
}

Segmentation::Segmentation( const Segmentation & other )
  : absolute_segment_adjustments( other.absolute_segment_adjustments ),
    segment_quantizer_adjustments( other.segment_quantizer_adjustments ),
    segment_filter_adjustments( other.segment_filter_adjustments ),
    map( other.map.width(), other.map.height() )
{
  map.copy_from( other.map );
}
