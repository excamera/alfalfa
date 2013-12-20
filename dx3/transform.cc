#include "macroblock_header.hh"
#include "block.hh"

template <>
void YBlock::set_dc_coefficient( const int16_t & val )
{
  coefficients_[ 0 ] = val;
  set_Y_without_Y2();
}

template <>
void Y2Block::walsh_transform( TwoDSubRange< YBlock > & output )
{
  assert( coded_ );
  assert( output.width() == 4 );
  assert( output.height() == 4 );

  array< int16_t, 16 > intermediate;

  for ( unsigned int i = 0; i < 4; i++ ) {
    int a1 = coefficients_[ i + 0 ] + coefficients_[ i + 12 ];
    int b1 = coefficients_[ i + 4 ] + coefficients_[ i + 8  ];
    int c1 = coefficients_[ i + 4 ] - coefficients_[ i + 8  ];
    int d1 = coefficients_[ i + 0 ] - coefficients_[ i + 12 ];

    intermediate[ i + 0  ] = a1 + b1;
    intermediate[ i + 4  ] = c1 + d1;
    intermediate[ i + 8  ] = a1 - b1;
    intermediate[ i + 12 ] = d1 - c1;
  }

  for ( unsigned int i = 0; i < 4; i++ ) {
    const uint8_t offset = i * 4;
    int a1 = intermediate[ offset + 0 ] + intermediate[ offset + 3 ];
    int b1 = intermediate[ offset + 1 ] + intermediate[ offset + 2 ];
    int c1 = intermediate[ offset + 1 ] - intermediate[ offset + 2 ];
    int d1 = intermediate[ offset + 0 ] - intermediate[ offset + 3 ];

    int a2 = a1 + b1;
    int b2 = c1 + d1;
    int c2 = a1 - b1;
    int d2 = d1 - c1;

    output.at( i, 0 ).set_dc_coefficient( (a2 + 3) >> 3 );
    output.at( i, 1 ).set_dc_coefficient( (b2 + 3) >> 3 );
    output.at( i, 2 ).set_dc_coefficient( (c2 + 3) >> 3 );
    output.at( i, 3 ).set_dc_coefficient( (d2 + 3) >> 3 );
  }
}

static inline int MUL_20091( const int a ) { return ((((a)*20091) >> 16) + (a)); }
static inline int MUL_35468( const int a ) { return (((a)*35468) >> 16); }

template <BlockType initial_block_type, class PredictionMode>
void Block< initial_block_type, PredictionMode >::idct( Raster::Block & output )
{
  assert( type_ == UV or type_ == Y_without_Y2 );

  array< int16_t, 16 > intermediate;

  /* Based on libav/ffmpeg vp8_idct_add_c */

  for ( int i = 0; i < 4; i++ ) {
    int t0 = coefficients_[ i + 0 ] + coefficients_[ i + 8 ];
    int t1 = coefficients_[ i + 0 ] - coefficients_[ i + 8 ];
    int t2 = MUL_35468( coefficients_[ i + 4 ] ) - MUL_20091( coefficients_[ i + 12 ] );
    int t3 = MUL_20091( coefficients_[ i + 4 ] ) + MUL_35468( coefficients_[ i + 12 ] );
    coefficients_[ i + 0 ] = 0;
    coefficients_[ i + 4 ] = 0;
    coefficients_[ i + 8 ] = 0;
    coefficients_[ i + 12 ] = 0;

    intermediate[ i * 4 + 0 ] = t0 + t3;
    intermediate[ i * 4 + 1 ] = t1 + t2;
    intermediate[ i * 4 + 2 ] = t1 - t2;
    intermediate[ i * 4 + 3 ] = t0 - t3;
  }

  for ( int i = 0; i < 4; i++ ) {
    int t0 = intermediate[ i + 0 ] + intermediate[ i + 8 ];
    int t1 = intermediate[ i + 0 ] - intermediate[ i + 8 ];
    int t2 = MUL_35468( intermediate[ i + 4 ] ) - MUL_20091( intermediate[ i + 12 ] );
    int t3 = MUL_20091( intermediate[ i + 4 ] ) + MUL_35468( intermediate[ i + 12 ] );

    output.at( 0, i ) = (t0 + t3 + 4) >> 3;
    output.at( 1, i ) = (t1 + t2 + 4) >> 3;
    output.at( 2, i ) = (t1 - t2 + 4) >> 3;
    output.at( 3, i ) = (t0 - t3 + 4) >> 3;
  }
}

void KeyFrameMacroblockHeader::dequantize( const KeyFrameHeader::DerivedQuantities & derived )
{
  /* is macroblock skipped? */
  if ( mb_skip_coeff_.get_or( false ) ) {
    return;
  }

  /* which quantizer are we using? */
  const Quantizer & the_quantizer( segment_id_.initialized()
				   ? derived.segment_quantizers.at( segment_id_.get() )
				   : derived.quantizer );

  if ( Y2_.coded() ) {
    Y2_.dequantize( the_quantizer );
  }

  Y_.forall( [&] ( YBlock & block ) { block.dequantize( the_quantizer ); } );
  U_.forall( [&] ( UVBlock & block ) { block.dequantize( the_quantizer ); } );
  V_.forall( [&] ( UVBlock & block ) { block.dequantize( the_quantizer ); } );
}

void KeyFrameMacroblockHeader::inverse_transform( Raster::Macroblock & macroblock )
{
  /* is macroblock skipped? */
  if ( mb_skip_coeff_.get_or( false ) ) {
    return;
  }

  /* transfer the Y2 block with WHT first, if necessary */
  if ( Y2_.coded() ) {
    Y2_.walsh_transform( Y_ );
  }

  /* decode the Y blocks */
  Y_.forall( [&] ( YBlock & block, const unsigned int column, const unsigned int row )
	     { block.idct( macroblock.Y_blocks.at( column, row ) ); } );

  /* U blocks */
  U_.forall( [&] ( UVBlock & block, const unsigned int column, const unsigned int row )
	     { block.idct( macroblock.U_blocks.at( column, row ) ); } );

  /* V blocks */
  V_.forall( [&] ( UVBlock & block, const unsigned int column, const unsigned int row )
	     { block.idct( macroblock.V_blocks.at( column, row ) ); } );
}
