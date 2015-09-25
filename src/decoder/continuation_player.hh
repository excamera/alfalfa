#ifndef CONTINUATION_PLAYER_HH
#define CONTINUATION_PLAYER_HH

#include "player.hh"
#include "frame.hh"

class SourcePlayer;

struct PreContinuation
{
  bool shown;
  MissingTracker missing;
  SourceHash source_hash;
  TargetHash target_hash;

  std::string continuation_name( void ) const;
};

class ContinuationPlayer : public FramePlayer
{
private:
  References prev_references_;
  bool shown_;
  RasterHandle cur_output_;
  UpdateTracker cur_updates_;
  bool on_key_frame_;
  Optional<KeyFrame> key_frame_;
  Optional<InterFrame> inter_frame_;

  template<class FrameType>
  Optional<RasterHandle> track_continuation_info( const FrameType & frame );

public:
  ContinuationPlayer( const uint16_t width, const uint16_t height );

  ContinuationPlayer( ContinuationPlayer && ) = default;
  ContinuationPlayer & operator=( ContinuationPlayer && ) = default;

  Optional<RasterHandle> decode( const SerializedFrame & frame );

  void apply_changes( Decoder & other ) const;

  bool need_gen_continuation( void ) const;

  PreContinuation make_pre_continuation( const SourcePlayer & source ) const;

  SerializedFrame make_continuation( const PreContinuation & pre,
                                     const SourcePlayer & source ) const;
  SerializedFrame operator-( const SourcePlayer & source ) const;
};

class SourcePlayer : public FramePlayer
{
friend class ContinuationPlayer;
private:
  bool need_continuation_, first_continuation_;

public:
  SourcePlayer( const ContinuationPlayer & original )
    : FramePlayer( original ),
      need_continuation_( true ), // Assume we're making a continuation frame unless we've shown otherwise
      first_continuation_( true )
  {}

  void set_need_continuation( bool b )
  {
    need_continuation_ = b;
  }

  bool need_continuation( void ) const
  {
    return need_continuation_;
  }

  bool first_continuation( void ) const
  {
    return first_continuation_;
  }

  Optional<RasterHandle> decode( const SerializedFrame & frame )
  {
    first_continuation_ = false;
    return FramePlayer::decode( frame );
  }

  void sync_changes( const ContinuationPlayer & target_player )
  {
    first_continuation_ = false;
    target_player.apply_changes( decoder_ );
  }
};

#endif
