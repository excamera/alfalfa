#ifndef CONTINUATION_PLAYER_HH
#define CONTINUATION_PLAYER_HH

#include "player.hh"
#include "continuation_state.hh"

class SourcePlayer;

class ContinuationPlayer : public FilePlayer
{
private:
  ContinuationState continuation_state_;
public:
  ContinuationPlayer( const std::string & file_name );

  SerializedFrame serialize_next( void );

  RasterHandle next_output( void );
  RasterHandle advance( void );

  void apply_changes( Decoder & other ) const;

  std::string get_frame_stats( void ) const;

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
