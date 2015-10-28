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
  pair<SerializedFrame, FrameInfo> serialize_next();
  RasterHandle next_output();
};

#endif
