#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder_state.hh"

using namespace std;

Decoder::Decoder( const uint16_t width, const uint16_t height, const Chunk & key_frame )
  : state_( KeyFrame( UncompressedChunk( key_frame, width, height ),
		      width, height ).header(),
	    width, height ),
    references_( width, height )
{}

bool Decoder::decode_frame( const Chunk & frame, RasterHandle & raster )
{
  /* parse uncompressed data chunk */
  UncompressedChunk uncompressed_chunk( frame, state_.width, state_.height );

  if ( uncompressed_chunk.key_frame() ) {
    const KeyFrame myframe = state_.parse_and_apply<KeyFrame>( uncompressed_chunk );

    myframe.decode( state_.quantizer_filter_adjustments, raster );

    myframe.copy_to( raster, references_ );

    return myframe.show();
  } else {
    const InterFrame myframe = state_.parse_and_apply<InterFrame>( uncompressed_chunk );

    myframe.decode( state_.quantizer_filter_adjustments, references_, raster );

    myframe.copy_to( raster, references_ );

    return myframe.show();
  }
}

References::References( const uint16_t width, const uint16_t height )
  : last( width, height ),
    golden( width, height ),
    alternative_reference( width, height )
{}

template <unsigned int size>
static void assign( SafeArray< Probability, size > & dest, const Array< Unsigned<8>, size > & src )
{
  for ( unsigned int i = 0; i < size; i++ ) {
    dest.at( i ) = src.at( i );
  }
}

void ProbabilityTables::update( const InterFrameHeader & header )
{
  coeff_prob_update( header );

  /* update intra-mode probabilities in inter macroblocks */
  if ( header.intra_16x16_prob.initialized() ) {
    assign( y_mode_probs, header.intra_16x16_prob.get() );
  }

  if ( header.intra_chroma_prob.initialized() ) {
    assign( uv_mode_probs, header.intra_chroma_prob.get() );
  }

  /* update motion vector component probabilities */
  for ( uint8_t i = 0; i < header.mv_prob_update.size(); i++ ) {
    for ( uint8_t j = 0; j < header.mv_prob_update.at( i ).size(); j++ ) {
      const auto & prob = header.mv_prob_update.at( i ).at( j );
      if ( prob.initialized() ) {
	motion_vector_probs.at( i ).at( j ) = prob.get();
      }
    }
  }
}

DecoderState::DecoderState( const KeyFrameHeader & header,
			    const unsigned int s_width,
			    const unsigned int s_height )
  : width( s_width ),
    height( s_height ),
    quantizer_filter_adjustments( header ),
    segmentation_map( Raster::macroblock_dimension( width ),
		      Raster::macroblock_dimension( height ) )
{}
