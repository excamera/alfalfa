#ifndef CONTINUATION_STATE_HH
#define CONTINUATION_STATE_HH

#include "decoder.hh"
#include "decoder_tracking.hh"
#include "serialized_frame.hh"
#include "frame.hh"

struct PreContinuation
{
  bool shown;
  MissingTracker missing;
  SourceHash source_hash;
  TargetHash target_hash;

  std::string continuation_name( void ) const;
};

class ContinuationState
{
private:
  References prev_references_;
  RasterHandle last_output_;
  UpdateTracker cur_updates_;
  bool shown_;

  // Put these in a union?
  bool on_key_frame_;
  Optional<KeyFrame> key_frame_;
  Optional<InterFrame> inter_frame_;

public:
  ContinuationState( const uint16_t width, const uint16_t height );
  ContinuationState( KeyFrame && frame, bool shown, const RasterHandle & raster,
                     const UpdateTracker & updates, const References & refs );
  ContinuationState( InterFrame && frame, bool shown, const RasterHandle & raster,
                     const UpdateTracker & updates, const References & refs );

  PreContinuation make_pre_continuation( const Decoder & target, const Decoder & source,
                                         const bool shown ) const;

  SerializedFrame make_continuation( const DecoderDiff & difference, const PreContinuation & pre ) const;

  void apply_last_frame( Decoder & source, const Decoder & target ) const;

  bool is_shown( void ) const;
  bool on_key_frame( void ) const;
  RasterHandle get_output( void ) const;
  Optional<RasterHandle> get_shown( void ) const;

  std::string get_frame_stats( void ) const;

  DependencyTracker get_frame_used( void ) const;
  UpdateTracker get_frame_updated( void ) const;
};

#endif
