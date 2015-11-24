#include "macroblock.hh"
#include "block.hh"
#include "safe_array.hh"
#include "transform_sse.hh"

template <>
void YBlock::set_dc_coefficient( const int16_t & val )
{
  coefficients_.set_dc_coefficient( val );
  set_Y_without_Y2();
}

void DCTCoefficients::set_dc_coefficient( const int16_t & val )
{
  coefficients_.at( 0 ) = val;
}

void DCTCoefficients::walsh_transform( SafeArray< SafeArray< DCTCoefficients, 4 >, 4 > & output ) const
{
  SafeArray< int16_t, 16 > intermediate;

  for ( unsigned int i = 0; i < 4; i++ ) {
    int a1 = coefficients_.at( i + 0 ) + coefficients_.at( i + 12 );
    int b1 = coefficients_.at( i + 4 ) + coefficients_.at( i + 8  );
    int c1 = coefficients_.at( i + 4 ) - coefficients_.at( i + 8  );
    int d1 = coefficients_.at( i + 0 ) - coefficients_.at( i + 12 );

    intermediate.at( i + 0  ) = a1 + b1;
    intermediate.at( i + 4  ) = c1 + d1;
    intermediate.at( i + 8  ) = a1 - b1;
    intermediate.at( i + 12 ) = d1 - c1;
  }

  for ( unsigned int i = 0; i < 4; i++ ) {
    const uint8_t offset = i * 4;
    int a1 = intermediate.at( offset + 0 ) + intermediate.at( offset + 3 );
    int b1 = intermediate.at( offset + 1 ) + intermediate.at( offset + 2 );
    int c1 = intermediate.at( offset + 1 ) - intermediate.at( offset + 2 );
    int d1 = intermediate.at( offset + 0 ) - intermediate.at( offset + 3 );

    int a2 = a1 + b1;
    int b2 = c1 + d1;
    int c2 = a1 - b1;
    int d2 = d1 - c1;

    output.at( i ).at( 0 ).set_dc_coefficient( (a2 + 3) >> 3 );
    output.at( i ).at( 1 ).set_dc_coefficient( (b2 + 3) >> 3 );
    output.at( i ).at( 2 ).set_dc_coefficient( (c2 + 3) >> 3 );
    output.at( i ).at( 3 ).set_dc_coefficient( (d2 + 3) >> 3 );
  }
}

#ifdef HAVE_SSE2

void DCTCoefficients::idct_add( VP8Raster::Block4 & output ) const
{

  vp8_short_idct4x4llm_mmx( &coefficients_.at( 0 ), &output.at( 0, 0 ), output.stride(), &output.at( 0, 0 ), output.stride() );
}

#else

static inline int MUL_20091( const int a ) { return ((((a)*20091) >> 16) + (a)); }
static inline int MUL_35468( const int a ) { return (((a)*35468) >> 16); }

void DCTCoefficients::idct_add( VP8Raster::Block4 & output ) const
{
  SafeArray< int16_t, 16 > intermediate;

  /* Based on libav/ffmpeg vp8_idct_add_c */

  for ( int i = 0; i < 4; i++ ) {
    int t0 = coefficients_.at( i + 0 ) + coefficients_.at( i + 8 );
    int t1 = coefficients_.at( i + 0 ) - coefficients_.at( i + 8 );
    int t2 = MUL_35468( coefficients_.at( i + 4 ) ) - MUL_20091( coefficients_.at( i + 12 ) );
    int t3 = MUL_20091( coefficients_.at( i + 4 ) ) + MUL_35468( coefficients_.at( i + 12 ) );

    intermediate.at( i * 4 + 0 ) = t0 + t3;
    intermediate.at( i * 4 + 1 ) = t1 + t2;
    intermediate.at( i * 4 + 2 ) = t1 - t2;
    intermediate.at( i * 4 + 3 ) = t0 - t3;
  }

  for ( int i = 0; i < 4; i++ ) {
    int t0 = intermediate.at( i + 0 ) + intermediate.at( i + 8 );
    int t1 = intermediate.at( i + 0 ) - intermediate.at( i + 8 );
    int t2 = MUL_35468( intermediate.at( i + 4 ) ) - MUL_20091( intermediate.at( i + 12 ) );
    int t3 = MUL_20091( intermediate.at( i + 4 ) ) + MUL_35468( intermediate.at( i + 12 ) );

    uint8_t *target = &output.at( 0, i );

    *target = clamp255( *target + ((t0 + t3 + 4) >> 3) );
    target++;
    *target = clamp255( *target + ((t1 + t2 + 4) >> 3) );
    target++;
    *target = clamp255( *target + ((t1 - t2 + 4) >> 3) );
    target++;
    *target = clamp255( *target + ((t0 - t3 + 4) >> 3) );
  }
}
#endif

template <BlockType initial_block_type, class PredictionMode>
void Block< initial_block_type, PredictionMode >::add_residue( VP8Raster::Block4 & output ) const
{
  for ( uint8_t i = 0; i < 16; i++ ) {
    output.at( i % 4, i / 4 ) = clamp255( output.at( i % 4, i / 4 )
					  + coefficients_.at( zigzag.at( i ) ) );
  }
}
