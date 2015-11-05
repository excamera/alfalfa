#include "tracking_player.hh"
#include "frame.hh"

using namespace std;

template<class FrameType>
pair<FrameInfo, Optional<RasterHandle>> TrackingPlayer::decode_and_info( const FrameType & frame,
                                                                         const Decoder & source,
                                                                         const FrameRawData & raw_data )
{
  pair<bool, RasterHandle> output = decoder_.decode_frame( frame );

  SourceHash source_hash = source.source_hash( frame.get_used() );
  TargetHash target_hash = decoder_.target_hash( frame.get_updated(),
                                                 output.second, output.first );

  // Check this is a sane frame
  assert( source.get_hash().can_decode( source_hash ) );

  return make_pair( FrameInfo( FrameName( source_hash, target_hash ), raw_data.offset, raw_data.length ),
                    make_optional( output.first, output.second ) );
}

pair<FrameInfo, Optional<RasterHandle>> TrackingPlayer::serialize_next()
{
  FrameRawData raw_data = get_next_frame();

  // Save the source so we can hash the parts of it that are used by
  // the next frame;
  Decoder source = decoder_;

  UncompressedChunk decompressed_frame = decoder_.decompress_frame( raw_data.chunk );
  if ( decompressed_frame.key_frame() ) {
    KeyFrame frame = decoder_.parse_frame<KeyFrame>( decompressed_frame );
    return decode_and_info( frame, source, raw_data );
  }
  else {
    InterFrame frame = decoder_.parse_frame<InterFrame>( decompressed_frame );
    return decode_and_info( frame, source, raw_data );
  }
}

RasterHandle TrackingPlayer::next_output( void )
{
  UncompressedChunk decompressed_frame = decoder_.decompress_frame( get_next_frame().chunk );

  if ( decompressed_frame.key_frame() ) {
    return decoder_.decode_frame( decoder_.parse_frame<KeyFrame>( decompressed_frame ) ).second;
  }
  else {
    return decoder_.decode_frame( decoder_.parse_frame<InterFrame>( decompressed_frame ) ).second;
  }
}
