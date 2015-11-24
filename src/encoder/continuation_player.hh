#ifndef CONTINUATION_PLAYER_HH
#define CONTINUATION_PLAYER_HH

#include "player.hh"
#include "frame.hh"
#include "frame_info.hh"

#include <vector>

class SourcePlayer;

class ContinuationPlayer : public FramePlayer
{
private:
  References prev_references_;
  bool shown_;
  RasterHandle cur_output_;

  bool on_key_frame_;
  Optional<KeyFrame> key_frame_;
  Optional<InterFrame> inter_frame_;

  template<class FrameType>
  Optional<RasterHandle> track_continuation_info( const FrameType & frame );

public:
  ContinuationPlayer( const uint16_t width, const uint16_t height );

  ContinuationPlayer( ContinuationPlayer && ) = default;
  ContinuationPlayer & operator=( ContinuationPlayer && ) = default;

  Optional<RasterHandle> decode( const Chunk & chunk );

  bool on_key_frame() const;

  SerializedFrame make_state_update( const SourcePlayer & source ) const;

  std::vector<SerializedFrame> make_reference_updates( const SourcePlayer & source ) const;

  SerializedFrame rewrite_inter_frame( const SourcePlayer & source ) const; 

  void apply_changes( SourcePlayer & source ) const;

  bool need_state_update( const SourcePlayer & source ) const;
};

class SourcePlayer : public FramePlayer
{
friend class ContinuationPlayer;
private:
  bool need_continuation_;

public:
  SourcePlayer( const ContinuationPlayer & original )
    : FramePlayer( original ),
      need_continuation_( false )
  {}

  void set_need_continuation( bool b )
  {
    need_continuation_ = b;
  }

  bool need_continuation() const
  {
    return need_continuation_;
  }

  bool operator==( const SourcePlayer & other) const
  {
    return FramePlayer::operator==( other ) and need_continuation_ == other.need_continuation_;
  }
};

#endif
