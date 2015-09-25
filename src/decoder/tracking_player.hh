#ifndef TRACKING_PLAYER_HH
#define TRACKING_PLAYER_HH

#include "player.hh"

class TrackingPlayer : public FilePlayer
{
private:
  template<class FrameType>
  SerializedFrame decode_and_serialize( const FrameType & frame, const Decoder & source,
                                        const Chunk & compressed_frame );

public:
  using FilePlayer::FilePlayer;

  SerializedFrame serialize_next( void );

  RasterHandle next_output( void );
};

#endif
