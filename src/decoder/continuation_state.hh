#ifndef CONTINUATION_STATE_HH
#define CONTINUATION_STATE_HH

#include "decoder.hh"
#include "decoder_tracking.hh"
#include "serialized_frame.hh"
#include "frame.hh"

class ContinuationState
{
private:
  References prev_references_;
  RasterHandle last_output_;
  bool shown_;

  // Put these in a union?
  bool on_key_frame_;
  Optional<KeyFrame> key_frame_;
  Optional<InterFrame> inter_frame_;

public:
  ContinuationState( const uint16_t width, const uint16_t height );
  ContinuationState( KeyFrame && frame, bool shown, const RasterHandle & raster,
                     const References & refs );
  ContinuationState( InterFrame && frame, bool shown, const RasterHandle & raster,
                     const References & refs );

  SerializedFrame make_continuation( const DecoderDiff & difference ) const;

  void apply_last_frame( Decoder & source, const Decoder & target ) const;

  bool is_shown( void ) const;
  RasterHandle get_output( void ) const;
  Optional<RasterHandle> get_shown( void ) const;

  std::string get_frame_stats( void ) const;

  DependencyTracker get_frame_used( void ) const;
  UpdateTracker get_frame_updated( void ) const;
};

#endif
