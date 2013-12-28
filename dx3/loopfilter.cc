#include "loopfilter.hh"
#include "frame_header.hh"
#include "macroblock_header.hh"

static inline uint8_t clamp63( const int input )
{
  if ( input > 63 ) return 63;
  if ( input < 0 ) return 0;
  return input;
}

FilterParameters::FilterParameters( const KeyFrameHeader & header )
  : type( header.filter_type ? LoopFilterType::Simple : LoopFilterType::Normal ),
    filter_level( header.loop_filter_level ),
    sharpness_level( header.sharpness_level )
{}

FilterParameters::FilterParameters( const uint8_t segment_id,
				    const KeyFrameHeader & header,
				    const Optional< UpdateSegmentation > & update_segmentation )
  : FilterParameters( header )
{
  if ( update_segmentation.initialized()
       and update_segmentation.get().segment_feature_data.initialized() ) {
    const auto & feature_data = update_segmentation.get().segment_feature_data.get();
    const auto & update = feature_data.loop_filter_update.at( segment_id );

    if ( update.initialized() ) {
      if ( feature_data.segment_feature_mode ) { /* absolute value */
	if ( update.get() < 0 or update.get() > 63 ) {
	  throw Invalid( "absolute loop-filter update with out-of-bounds value" );
	}
	filter_level = update.get();
      } else { /* delta */
	filter_level += update.get();
      }
    }
  }
}

static uint8_t mode_category( const reference_frame macroblock_reference_frame,
			      const intra_mbmode macroblock_y_mode )
{
  if ( macroblock_reference_frame == CURRENT_FRAME and macroblock_y_mode == B_PRED ) {
    return 0;
    /*  } else if ( macroblock_y_mode == ZEROMV ) {
    return 1;
  } else if ( macroblock_y_mode == SPLITMV ) {
  return 3; */
  } else {
    return 2;
  }
}

FilterParameters FilterParameters::adjust( const SafeArray< int8_t, num_reference_frames > & ref_adjustments,
					   const SafeArray< int8_t, 4 > & mode_adjustments,
					   const reference_frame macroblock_reference_frame,
					   const intra_mbmode macroblock_y_mode ) const
{
  FilterParameters ret( *this );

  ret.filter_level += ref_adjustments.at( macroblock_reference_frame )
    + mode_adjustments.at( mode_category( macroblock_reference_frame, macroblock_y_mode ) );

  return ret;
}

SimpleLoopFilter::SimpleLoopFilter( const FilterParameters & params )
  : filter_level_( clamp63( params.filter_level ) ),
    interior_limit_( filter_level_ ),
    macroblock_edge_limit_(),
    subblock_edge_limit_()
{
  assert( params.type == LoopFilterType::Simple );

  if ( params.sharpness_level ) {
    interior_limit_ >>= params.sharpness_level > 4 ? 2 : 1;

    if ( interior_limit_ > 9 - params.sharpness_level ) {
      interior_limit_ = 9 - params.sharpness_level;
    }
  }

  if ( interior_limit_ < 1 ) {
    interior_limit_ = 1;
  }

  macroblock_edge_limit_ = ( ( params.filter_level + 2 ) * 2 ) + interior_limit_;
  subblock_edge_limit_ = ( params.filter_level * 2 ) + interior_limit_;
}

NormalLoopFilter::NormalLoopFilter( const bool key_frame,
				    const FilterParameters & params )
  : simple_( params ),
    high_edge_variance_threshold_( params.filter_level >= 15 )
{
  assert( params.type == LoopFilterType::Normal );

  if ( params.filter_level >= 40 ) {
    high_edge_variance_threshold_++;
  }

  if ( params.filter_level >= 20 and (not key_frame) ) {
    high_edge_variance_threshold_++;
  }
}

void KeyFrameMacroblockHeader::loopfilter( const KeyFrameHeader::DerivedQuantities &  )
{
  
}
