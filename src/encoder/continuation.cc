#include "frame.hh"
#include "macroblock.hh"

/* Based on libvpx vp8_short_fdct4x4_c */
template <BlockType initial_block_type, class PredictionMode>
void Block<initial_block_type, PredictionMode>::fdct( const Raster::Block4 & input )
{
  for ( uint8_t row = 0; row < 4; row++ ) {
    const int16_t a1 = (input.contents().at( 0, row ) + input.contents().at( 3, row )) * 8;
    const int16_t b1 = (input.contents().at( 1, row ) + input.contents().at( 2, row )) * 8;
    const int16_t c1 = (input.contents().at( 1, row ) - input.contents().at( 2, row )) * 8;
    const int16_t d1 = (input.contents().at( 0, row ) - input.contents().at( 3, row )) * 8;

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
  IsolatedRasterMacroblock( const Raster::Macroblock & source )
  {
    raster_.macroblock( 0, 0 ) = source;
  }

  bool operator==( const Raster::Macroblock & other ) const
  {
    return raster_.macroblock( 0, 0 ) == other;
  }
};

template <>
void InterFrameMacroblock::rewrite_as_intra( const Quantizer & quantizer, Raster::Macroblock & raster )
{
  assert( inter_coded() );

  const IsolatedRasterMacroblock target( raster );

  /* set intra coded */
  header_.is_inter_mb = false;
  Y2_.set_prediction_mode( B_PRED );
  Y_.forall( [&] ( YBlock & block ) {
      block.set_prediction_mode( B_DC_PRED );
      block.set_Y_without_Y2();
    } );
  U_.at( 0, 0 ).set_prediction_mode( DC_PRED );

  /* attempt decoding and measure residue */
  reconstruct_intra( quantizer, raster );

  /* examine Y first */
  
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
					   macroblock.rewrite_as_intra( quantizer, raster.macroblock( column, row ) );
					 } else {
					   macroblock.reconstruct_intra( quantizer,
									 raster.macroblock( column, row ) );
					 } } );

  loopfilter( quantizer_filter_adjustments, raster );
}
