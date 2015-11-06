#ifndef TRACKING_PLAYER_HH
#define TRACKING_PLAYER_HH

#include "player.hh"

class TrackingPlayer : public FilePlayer
{
private:
  template<class FrameType>
  pair<FrameInfo, Optional<RasterHandle>> decode_and_info( const FrameType & frame, const Decoder & source,
                                                           const FrameRawData & raw_data );

public:
  using FilePlayer::FilePlayer;
  pair<FrameInfo, Optional<RasterHandle>> serialize_next();
  RasterHandle next_output();
};

#endif
