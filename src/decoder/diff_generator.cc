#include "diff_generator.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"

using namespace std;

DiffGenerator::DiffGenerator( const uint16_t width, const uint16_t height )
  : Decoder( width, height ),
    on_key_frame_ { true }
{}

bool DiffGenerator::decode_frame( const Chunk & frame, RasterHandle & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  if ( uncompressed_chunk.key_frame() ) {
    on_key_frame_ = true;

    key_frame_ = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    key_frame_.get().decode( state_.segmentation, raster );

    // Store copy of the raster for diffs before loopfilter is applied
    references_.update_continuation( raster );

    key_frame_.get().loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    key_frame_.get().copy_to( raster, references_ );

    return key_frame_.get().show_frame();
  } else {
    on_key_frame_ = false;

    inter_frame_ = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    inter_frame_.get().decode( state_.segmentation, references_, raster );

    references_.update_continuation( raster );

    inter_frame_.get().loopfilter( state_.segmentation, state_.filter_adjustments, raster );

    inter_frame_.get().copy_to( raster, references_ );

    return inter_frame_.get().show_frame();
  }
}

// This needs to be made const, which means rewriting rewrite_as_diff into make_continuation_frame
vector<uint8_t> DiffGenerator::operator-( const DiffGenerator & source_decoder ) const
{
  if ( on_key_frame_ ) {
    return key_frame_.get().serialize( state_.probability_tables );
  } else {
    InterFrame diff_frame( inter_frame_.get(), source_decoder.state_, state_,
			   source_decoder.references_.continuation, references_.continuation );
    
    diff_frame.optimize_continuation_coefficients();

    return diff_frame.serialize( state_.probability_tables );
  }
}
