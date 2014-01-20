#include "frame.hh"
#include "macroblock.hh"

/* Based on libvpx vp8_short_fdct4x4_c */
template <BlockType initial_block_type, class PredictionMode>
void Block<initial_block_type, PredictionMode>::fdct( const SafeArray< SafeArray< int16_t, 4 >, 4 > & input )
{
  for ( uint8_t row = 0; row < 4; row++ ) {
    const int16_t a1 = (input.at( row ).at( 0 ) + input.at( row ).at( 3 )) * 8;
    const int16_t b1 = (input.at( row ).at( 1 ) + input.at( row ).at( 2 )) * 8;
    const int16_t c1 = (input.at( row ).at( 1 ) - input.at( row ).at( 2 )) * 8;
    const int16_t d1 = (input.at( row ).at( 0 ) - input.at( row ).at( 3 )) * 8;

    coefficients_.at( row * 4 + 0 ) = a1 + b1;
    coefficients_.at( row * 4 + 2 ) = a1 - b1;
    coefficients_.at( row * 4 + 1 ) = (c1 * 2217 + d1 * 5352 + 14500) >> 12;
    coefficients_.at( row * 4 + 3 ) = (d1 * 2217 - c1 * 5352 +  7500) >> 12;
  }

  for ( uint8_t i = 0; i < 4; i++ ) {
    const int16_t a1 = coefficients_.at( i + 0 ) + coefficients_.at( i + 12 );
    const int16_t b1 = coefficients_.at( i + 4 ) + coefficients_.at( i + 8 );
    const int16_t c1 = coefficients_.at( i + 4 ) - coefficients_.at( i + 8 );
    const int16_t d1 = coefficients_.at( i + 0 ) - coefficients_.at( i + 12 );

    coefficients_.at( i + 0 ) = (a1 + b1 + 7) >> 4;
    coefficients_.at( i + 8 ) = (a1 - b1 + 7) >> 4;
    coefficients_.at( i + 4 ) = ( (c1 * 2217 + d1 * 5352 + 12000) >> 16 ) + ( d1 != 0 );
    coefficients_.at( i + 12) = (d1 * 2217 - c1 * 5352 + 51000) >> 16;
  }
}

template <BlockType initial_block_type, class PredictionMode>
void Block< initial_block_type, PredictionMode >::apply_pixel_adjustment( Raster::Block4 & output ) const
{
  assert( type_ == UV or type_ == Y_without_Y2 );
  assert( has_pixel_adjustment() );

  for ( uint8_t row = 0; row < 4; row++ ) {
    for ( uint8_t column = 0; column < 4; column++ ) {
      output.at( column, row ) = clamp255( output.at( column, row )
					   + static_cast<int8_t>( pixel_adjustments_.at( row * 4 + column ) ) );
    }
  }
}

template <unsigned int size>
Raster::Block<size> & Raster::Block<size>::operator=( const Raster::Block<size> & other )
{
  contents_.copy( other.contents_ );
  return *this;
}

template <unsigned int size>
bool Raster::Block<size>::operator==( const Raster::Block<size> & other ) const
{
  return contents_ == other.contents_;
}

template <unsigned int size>
SafeArray< SafeArray< int16_t, size >, size > Raster::Block<size>::operator-( const Raster::Block<size> & other ) const
{
  SafeArray< SafeArray< int16_t, size >, size > ret;
  contents_.forall_ij( [&] ( const uint8_t & val, const unsigned int column, const unsigned int row ) {
      ret.at( row ).at( column ) = int16_t( val ) - int16_t( other.at( column, row ) );
    } );
  return ret;
}

Raster::Macroblock & Raster::Macroblock::operator=( const Raster::Macroblock & other )
{
  Y = other.Y;
  U = other.U;
  V = other.V;
  return *this;
}

bool Raster::Macroblock::operator==( const Raster::Macroblock & other ) const
{
  return (Y == other.Y) and (U == other.U) and (V == other.V);
}

class IsolatedRasterMacroblock
{
private:
  Raster raster_ { 16, 16 };

public:
  const Raster::Macroblock & mb( void ) const { return raster_.macroblock( 0, 0 ); }
  Raster::Macroblock & mb( void ) { return raster_.macroblock( 0, 0 ); }

  IsolatedRasterMacroblock( const Raster::Macroblock & source )
  {
    mb() = source;
  }

  bool operator==( const Raster::Macroblock & other ) const
  {
    return mb() == other;
  }
};

template <>
void InterFrameMacroblock::rewrite_as_intra( Raster::Macroblock & raster )
{
  assert( inter_coded() );

  const IsolatedRasterMacroblock target( raster );

  /* set prediction modes */
  Y2_.set_prediction_mode( B_PRED );
  Y_.forall( [&] ( YBlock & block ) {
      block.set_prediction_mode( B_DC_PRED );
      block.set_Y_without_Y2();
    } );
  U_.at( 0, 0 ).set_prediction_mode( DC_PRED );

  /* measure the Y residue */
  Y_.forall_ij( [&] ( YBlock & block, unsigned int column, unsigned int row ) {
      /* Intra-predict */
      raster.Y_sub.at( column, row ).intra_predict( block.prediction_mode() );

      /* Get the residue */
      const auto residue = target.mb().Y_sub.at( column, row ) - raster.Y_sub.at( column, row );

      /* Transform the residue */
      block.fdct( residue );

      /* Find any necessary pixel adjustments */
      raster.Y_sub.at( column, row ).intra_predict( block.prediction_mode() );
      block.idct_add( raster.Y_sub.at( column, row ) );
      for ( uint8_t pixel_row = 0; pixel_row < 4; pixel_row++ ) {
	for ( uint8_t pixel_col = 0; pixel_col < 4; pixel_col++ ) {
	  const int8_t adjustment = target.mb().Y_sub.at( column, row ).at( pixel_col, pixel_row )
	    - raster.Y_sub.at( column, row ).at( pixel_col, pixel_row );
	  if ( abs( adjustment ) > 1 ) {
	    throw Exception( "rewrite_as_intra", "pixel adjustment too large: " + to_string( adjustment ) );
	  }

	  if ( adjustment ) {
	    block.set_has_pixel_adjustment( true );
	    block.mutable_pixel_adjustments().at( pixel_row * 4 + pixel_col ) = static_cast<PixelAdjustment>( adjustment );
	  }
	}
      }

      /* Question: is the residue now round-trip? */
      raster.Y_sub.at( column, row ).intra_predict( block.prediction_mode() );
      block.idct_add( raster.Y_sub.at( column, row ) );
      if ( block.has_pixel_adjustment() ) {
	block.apply_pixel_adjustment( raster.Y_sub.at( column, row ) );
      }

      if ( target.mb().Y_sub.at( column, row ) == raster.Y_sub.at( column, row ) ) {
	//	printf( "Match.\n" );
      } else {
	printf( "Mismatch. Wanted:\n" );

	for ( int i = 0; i < 4; i++ ) {
	  for ( int j = 0; j < 4; j++ ) {
	    printf( " %u", target.mb().Y_sub.at( column, row ).at( j, i ) );
	  }
	  printf( "\n" );
	}

	printf( "\n Got:\n" );

	for ( int i = 0; i < 4; i++ ) {
	  for ( int j = 0; j < 4; j++ ) {
	    printf( " %u", raster.Y_sub.at( column, row ).at( j, i ) );
	  }
	  printf( "\n" );
	}
      }
    } );
}

template <>
void InterFrame::rewrite_as_intra( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
				   const References & references,
				   Raster & raster )
{
  const Quantizer frame_quantizer( header_.quant_indices );
  const auto segment_quantizers = calculate_segment_quantizers( quantizer_filter_adjustments );

  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( InterFrameMacroblock & macroblock,
					    const unsigned int column,
					    const unsigned int row ) {
					 const auto & quantizer = header_.update_segmentation.initialized()
					   ? segment_quantizers.at( macroblock.segment_id() )
					   : frame_quantizer;
					 if ( macroblock.inter_coded() ) {
					   macroblock.reconstruct_inter( quantizer,
									 references,
									 raster.macroblock( column, row ) );
					   macroblock.rewrite_as_intra( raster.macroblock( column, row ) );
					 } else {
					   macroblock.reconstruct_intra( quantizer,
									 raster.macroblock( column, row ) );
					 } } );

  loopfilter( quantizer_filter_adjustments, raster );
}
