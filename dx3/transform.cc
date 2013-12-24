#include "macroblock_header.hh"
#include "block.hh"
#include "safe_array.hh"

template <>
void YBlock::set_dc_coefficient( const int16_t & val )
{
  coefficients_.at( 0 ) = val;
  set_Y_without_Y2();
}

template <>
void Y2Block::walsh_transform( TwoDSubRange< YBlock, 4, 4 > & output )
{
  assert( coded_ );
  assert( output.width() == 4 );
  assert( output.height() == 4 );

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

    output.at( i, 0 ).set_dc_coefficient( (a2 + 3) >> 3 );
    output.at( i, 1 ).set_dc_coefficient( (b2 + 3) >> 3 );
    output.at( i, 2 ).set_dc_coefficient( (c2 + 3) >> 3 );
    output.at( i, 3 ).set_dc_coefficient( (d2 + 3) >> 3 );
  }
}

static inline int MUL_20091( const int a ) { return ((((a)*20091) >> 16) + (a)); }
static inline int MUL_35468( const int a ) { return (((a)*35468) >> 16); }

template <BlockType initial_block_type, class PredictionMode>
void Block< initial_block_type, PredictionMode >::idct( Raster::Block4 & output )
{
  assert( type_ == UV or type_ == Y_without_Y2 );

  SafeArray< int16_t, 16 > intermediate;

  /* Based on libav/ffmpeg vp8_idct_add_c */

  for ( int i = 0; i < 4; i++ ) {
    int t0 = coefficients_.at( i + 0 ) + coefficients_.at( i + 8 );
    int t1 = coefficients_.at( i + 0 ) - coefficients_.at( i + 8 );
    int t2 = MUL_35468( coefficients_.at( i + 4 ) ) - MUL_20091( coefficients_.at( i + 12 ) );
    int t3 = MUL_20091( coefficients_.at( i + 4 ) ) + MUL_35468( coefficients_.at( i + 12 ) );
    coefficients_.at( i + 0 ) = 0;
    coefficients_.at( i + 4 ) = 0;
    coefficients_.at( i + 8 ) = 0;
    coefficients_.at( i + 12 ) = 0;

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

    output.at( 0, i ) = clamp255( output.at( 0, i ) + ((t0 + t3 + 4) >> 3) );
    output.at( 1, i ) = clamp255( output.at( 1, i ) + ((t1 + t2 + 4) >> 3) );
    output.at( 2, i ) = clamp255( output.at( 2, i ) + ((t1 - t2 + 4) >> 3) );
    output.at( 3, i ) = clamp255( output.at( 3, i ) + ((t0 - t3 + 4) >> 3) );
  }
}

void KeyFrameMacroblockHeader::intra_predict_and_inverse_transform( void )
{
  /* Chroma */
  raster_.get()->U.intra_predict( uv_prediction_mode() );
  U_.forall_ij( [&] ( UVBlock & block, const unsigned int column, const unsigned int row )
		{ block.idct( raster_.get()->U_sub.at( column, row ) ); } );

  raster_.get()->V.intra_predict( uv_prediction_mode() );
  V_.forall_ij( [&] ( UVBlock & block, const unsigned int column, const unsigned int row )
		{ block.idct( raster_.get()->V_sub.at( column, row ) ); } );

  /* Luma */
  if ( Y2_.prediction_mode() == B_PRED ) {
    /* Prediction and inverse transform done in line! */
    Y_.forall_ij( [&] ( YBlock & block, const unsigned int column, const unsigned int row ) {
	raster_.get()->Y_sub.at( column, row ).intra_predict( block.prediction_mode() );
	block.idct( raster_.get()->Y_sub.at( column, row ) ); } );
  } else {
    raster_.get()->Y.intra_predict( Y2_.prediction_mode() );

    /* transfer the Y2 block with WHT first, if necessary */
    if ( Y2_.coded() ) {
      Y2_.walsh_transform( Y_ );
    }

    Y_.forall_ij( [&] ( YBlock & block, const unsigned int column, const unsigned int row )
		  { block.idct( raster_.get()->Y_sub.at( column, row ) ); } );
  }
}
