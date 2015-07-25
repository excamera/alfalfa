#ifndef CONTINUATION_PLAYER_HH
#define CONTINUATION_PLAYER_HH

#include "player.hh"
#include "continuation_state.hh"

class ContinuationPlayer : public FilePlayer
{
private:
  ContinuationState continuation_state_;
public:
  ContinuationPlayer( const std::string & file_name );

  SerializedFrame serialize_next( void );

  RasterHandle next_output( void );
  RasterHandle advance( void );

  std::string get_frame_stats( void ) const;

  SerializedFrame operator-( const FramePlayer & source ) const;
};


#endif
