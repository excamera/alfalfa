#ifndef REQUEST_TYPE_HH
#define REQUEST_TYPE_HH

enum class AlfalfaRequestType
{
  GetTrackSize,
  GetFrameFrameInfo,
  GetFrameTrackData,
  GetRaster,
  GetFramesTrack,
  GetFramesReverse,
  GetFramesSwitch,
  GetQualityDataByOriginalRaster,
  GetFramesByOutputHash,
  GetFramesByDecoderHash,
  GetTrackIds,
  GetTrackDataByFrameId,
  GetSwitchesEndingWithFrame,
  GetChunk,
  GetVideoWidth,
  GetVideoHeight
};

#endif
