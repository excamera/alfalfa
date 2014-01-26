#include <iostream>

#include "ivf.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"
#include "display.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  try {
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " SOURCE TARGET" << endl;
      return EXIT_FAILURE;
    }

    IVF source( argv[ 1 ] );
    IVF target( argv[ 2 ] );

    if ( source.fourcc() != "VP80"
	 or target.fourcc() != "VP80" ) {
      throw Unsupported( "not a VP8 file" );
    }

    /* make sure sizes agree */
    if ( target.width() != source.width()
	 or target.height() != source.height() ) {
      throw Unsupported( "stream size mismatch" );
    }

    uint32_t frame_no = 0;

    /* make sure first frame is a keyframe */
    if ( not UncompressedChunk( target.frame( frame_no ), target.width(), target.height() ).key_frame() ) {
      throw Unsupported( "target does not begin with keyframe" );
    }

    if ( not UncompressedChunk( source.frame( frame_no ), source.width(), source.height() ).key_frame() ) {
      throw Unsupported( "source does not begin with keyframe" );
    }

    DecoderState source_decoder_state( source.width(), source.height() ),
      target_decoder_state( source.width(), source.height() );
    References source_references( source.width(), source.height() ),
      target_references( target.width(), target.height() );

    uint32_t frame_count = source.frame_count();

    /* write IVF header */
    cout << "DKIF";
    cout << uint8_t(0) << uint8_t(0); /* version */
    cout << uint8_t(32) << uint8_t(0); /* header length */
    cout << "VP80"; /* fourcc */
    cout << uint8_t(source.width() & 0xff) << uint8_t((source.width() >> 8) & 0xff); /* width */
    cout << uint8_t(source.height() & 0xff) << uint8_t((source.height() >> 8) & 0xff); /* height */
    cout << uint8_t(1) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* bogus frame rate */
    cout << uint8_t(1) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* bogus time scale */

    const uint32_t le_num_frames = htole32( frame_count - frame_no );
    cout << string( reinterpret_cast<const char *>( &le_num_frames ), sizeof( le_num_frames ) ); /* num frames */

    cout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */

    for ( uint32_t i = frame_no; i < frame_count; i++ ) {
      RasterHandle source_raster( source.width(), source.height() ),
	target_raster( source.width(), source.height() ),
	source_raster_preloop( source.width(), source.height() );

      /* decode the source */
      UncompressedChunk whole_source( source.frame( i ), source.width(), source.height() );
      if ( whole_source.key_frame() ) {
	KeyFrame parsed_frame = source_decoder_state.parse_and_apply<KeyFrame>( whole_source );
	parsed_frame.decode( source_decoder_state.quantizer_filter_adjustments, source_raster_preloop, false );
	source_raster.get().copy( source_raster_preloop );
	parsed_frame.loopfilter( source_decoder_state.quantizer_filter_adjustments, source_raster );
	parsed_frame.copy_to( source_raster, source_references );
      } else {
	const InterFrame parsed_frame = source_decoder_state.parse_and_apply<InterFrame>( whole_source );
	parsed_frame.decode( source_decoder_state.quantizer_filter_adjustments,
			     source_references, source_raster_preloop, false );
	source_raster.get().copy( source_raster_preloop );
	parsed_frame.loopfilter( source_decoder_state.quantizer_filter_adjustments, source_raster );
	parsed_frame.copy_to( source_raster, source_references );
      }

      vector< uint8_t > serialized_frame;

      /* now decode and rewrite the target */
      UncompressedChunk whole_target( target.frame( i ), target.width(), target.height() );
      if ( whole_target.key_frame() ) {
	const KeyFrame parsed_frame = target_decoder_state.parse_and_apply<KeyFrame>( whole_target );
	parsed_frame.decode( target_decoder_state.quantizer_filter_adjustments, target_raster );
	parsed_frame.copy_to( target_raster, target_references );

	serialized_frame = parsed_frame.serialize( target_decoder_state.probability_tables );
	fprintf( stderr, "Frame %u, original length: %lu bytes. Serialized length: %lu bytes.\n",
		 i, target.frame( i ).size(), serialized_frame.size() );
      } else {
	InterFrame parsed_frame = target_decoder_state.parse_and_apply<InterFrame>( whole_target );

	parsed_frame.rewrite_as_diff( source_decoder_state, target_decoder_state,
				      target_references, source_raster_preloop, target_raster );

	//	parsed_frame.decode( target_decoder_state.quantizer_filter_adjustments, target_references, target_raster );


	parsed_frame.loopfilter( target_decoder_state.quantizer_filter_adjustments, target_raster );

	parsed_frame.copy_to( target_raster, target_references );

	parsed_frame.optimize_continuation_coefficients();

	serialized_frame = parsed_frame.serialize( target_decoder_state.probability_tables );

	fprintf( stderr, "Frame %u, original length: %lu bytes. Diff length: %lu bytes.\n",
		 i, target.frame( i ).size(), serialized_frame.size() );
      }

      /* write size of frame */
      const uint32_t le_size = htole32( serialized_frame.size() );
      cout << string( reinterpret_cast<const char *>( &le_size ), sizeof( le_size ) ); /* size */

      cout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */
      cout << uint8_t(0) << uint8_t(0) << uint8_t(0) << uint8_t(0); /* fill out header */

      /* write the frame */
      fwrite( &serialized_frame.at( 0 ), serialized_frame.size(), 1, stdout );
    }
  } catch ( const Exception & e ) {
    e.perror( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
