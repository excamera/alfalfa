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

template <BlockType initial_block_type, class PredictionMode>
static void rewrite_block_as_intra( Block<initial_block_type, PredictionMode> & block,
				    const Raster::Block4 & prediction,
				    const Raster::Block4 & target,
				    Raster::Block4 & raster )
{
  /* Get the residue */
  const auto residue = target - prediction;

  /* Transform the residue */
  block.fdct( residue );

  /* Find any necessary pixel adjustments */
  raster = prediction;
  block.idct_add( raster );
  for ( uint8_t pixel_row = 0; pixel_row < 4; pixel_row++ ) {
    for ( uint8_t pixel_col = 0; pixel_col < 4; pixel_col++ ) {
      const int8_t adjustment = target.at( pixel_col, pixel_row ) - raster.at( pixel_col, pixel_row );
      if ( abs( adjustment ) > 1 ) {
	throw Exception( "rewrite_block_as_intra", "pixel adjustment too large: " + to_string( adjustment ) );
      }

      if ( adjustment ) {
	block.set_has_pixel_adjustment( true );
	block.mutable_pixel_adjustments().at( pixel_row * 4 + pixel_col ) = static_cast<PixelAdjustment>( adjustment );
      }
    }
  }

  raster = target;
}

template <>
void InterFrameMacroblock::rewrite_as_intra( Raster::Macroblock & raster )
{
  assert( inter_coded() );

  if ( mb_skip_coeff_.initialized() ) {
    mb_skip_coeff_.get() = false;
  }

  continuation_ = true;

  /* set prediction modes */
  Y2_.set_prediction_mode( B_PRED );
  Y2_.set_if_coded();
  Y_.forall( [&] ( YBlock & block ) {
      block.set_prediction_mode( B_DC_PRED );
      block.set_Y_without_Y2();
    } );
  U_.at( 0, 0 ).set_prediction_mode( DC_PRED );

  const IsolatedRasterMacroblock target( raster );

  /* make the predictions */
  Y_.forall_ij( [&] ( YBlock & block, unsigned int column, unsigned int row ) {
      raster.Y_sub.at( column, row ).intra_predict( block.prediction_mode() );
    } );

  /* rewrite the Y subblocks */
  Y_.forall_ij( [&] ( YBlock & block, unsigned int column, unsigned int row ) {
      raster.Y_sub.at( column, row ).intra_predict( block.prediction_mode() );
      const IsolatedRasterMacroblock predicted( raster );
      rewrite_block_as_intra( block,
			      predicted.mb().Y_sub.at( column, row ),
			      target.mb().Y_sub.at( column, row ),
			      raster.Y_sub.at( column, row ) );
    } );

  raster.U.intra_predict( uv_prediction_mode() );
  raster.V.intra_predict( uv_prediction_mode() );
  const IsolatedRasterMacroblock predicted( raster );

  U_.forall_ij( [&] ( UVBlock & block, unsigned int column, unsigned int row ) {
      rewrite_block_as_intra( block,
			      predicted.mb().U_sub.at( column, row ),
			      target.mb().U_sub.at( column, row ),
			      raster.U_sub.at( column, row ) );
    } );

  V_.forall_ij( [&] ( UVBlock & block, unsigned int column, unsigned int row ) {
      rewrite_block_as_intra( block,
			      predicted.mb().V_sub.at( column, row ),
			      target.mb().V_sub.at( column, row ),
			      raster.V_sub.at( column, row ) );
    } );
}

template <>
void InterFrame::rewrite_as_intra( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
				   const References & references,
				   Raster & raster )
{
  assert( not continuation_header_.initialized() );
  continuation_header_.initialize( true, true, true );

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
