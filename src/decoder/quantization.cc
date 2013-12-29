#include "quantization.hh"
#include "frame_header.hh"
#include "block.hh"
#include "macroblock_header.hh"
#include "safe_array.hh"

using namespace std;

static const SafeArray< int16_t, 128 > dc_qlookup =
  {{ 4,   5,   6,   7,   8,   9,  10,  10,   11,  12,  13,  14,  15,
     16,  17,  17,  18,  19,  20,  20,  21,   21,  22,  22,  23,  23,
     24,  25,  25,  26,  27,  28,  29,  30,   31,  32,  33,  34,  35,
     36,  37,  37,  38,  39,  40,  41,  42,   43,  44,  45,  46,  46,
     47,  48,  49,  50,  51,  52,  53,  54,   55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  65,  66,  67,   68,  69,  70,  71,  72,
     73,  74,  75,  76,  76,  77,  78,  79,   80,  81,  82,  83,  84,
     85,  86,  87,  88,  89,  91,  93,  95,   96,  98, 100, 101, 102,
     104, 106, 108, 110, 112, 114, 116, 118, 122, 124, 126, 128, 130,
     132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157 }};

static const SafeArray< int16_t, 128 > ac_qlookup =
  {{ 4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,
     17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
     30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
     43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,
     56,  57,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,  78,
     80,  82,  84,  86,  88,  90,  92,  94,  96,  98, 100, 102, 104,
     106, 108, 110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137,
     140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173, 177,
     181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229,
     234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284 }};

static inline int16_t clamp_q( const int16_t q )
{
  if ( q < 0 ) return 0;
  if ( q > 127 ) return 127;

  return q;
}

Quantizer::Quantizer( const QuantIndices & quant_indices )
  : y_ac(  ac_qlookup.at( clamp_q( quant_indices.y_ac_qi ) ) ),
    y_dc(  dc_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.y_dc.get_or( 0 ) ) ) ),
    y2_ac( ac_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.y2_ac.get_or( 0 ) ) ) * 155/100 ),
    y2_dc( dc_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.y2_dc.get_or( 0 ) ) ) * 2 ),
    uv_ac( ac_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.uv_ac.get_or( 0 ) ) ) ),
    uv_dc( dc_qlookup.at( clamp_q( quant_indices.y_ac_qi + quant_indices.uv_dc.get_or( 0 ) ) ) )
{
  if ( y2_ac < 8 ) y2_ac = 8;
  if ( uv_dc > 132 ) uv_dc = 132;
}

static QuantIndices apply_segment_updates( const uint8_t segment_id,
					   const QuantIndices & quant_indices,
					   const Optional< UpdateSegmentation > & update_segmentation )
{
  QuantIndices ret( quant_indices );

  if ( update_segmentation.initialized()
       and update_segmentation.get().segment_feature_data.initialized() ) {
    const auto & feature_data = update_segmentation.get().segment_feature_data.get();
    const auto & update = feature_data.quantizer_update.at( segment_id );

    if ( update.initialized() ) {
      if ( feature_data.segment_feature_mode ) { /* absolute value */
	if ( update.get() < 0 ) {
	  throw Invalid( "absolute quantizer update with negative value" );
	}
	ret.y_ac_qi = static_cast<uint8_t>( update.get() );
      } else { /* delta */
	ret.y_ac_qi = ret.y_ac_qi + update.get();
      }
    }
  }

  return ret;
}

Quantizer::Quantizer( const uint8_t segment_id,
		      const QuantIndices & quant_indices,
		      const Optional< UpdateSegmentation > & update_segmentation )
  : Quantizer( apply_segment_updates( segment_id, quant_indices, update_segmentation ) )
{}

template <>
void Y2Block::dequantize( const Quantizer & quantizer )
{
  assert( coded_ );

  coefficients_.at( 0 ) *= quantizer.y2_dc;
  for ( uint8_t i = 1; i < 16; i++ ) {
    coefficients_.at( i ) *= quantizer.y2_ac;
  }
}

template <>
void YBlock::dequantize( const Quantizer & quantizer )
{
  coefficients_.at( 0 ) *= quantizer.y_dc;
  for ( uint8_t i = 1; i < 16; i++ ) {
    coefficients_.at( i ) *= quantizer.y_ac;
  }
}

template <>
void UVBlock::dequantize( const Quantizer & quantizer )
{
  coefficients_.at( 0 ) *= quantizer.uv_dc;
  for ( uint8_t i = 1; i < 16; i++ ) {
    coefficients_.at( i ) *= quantizer.uv_ac;
  }
}

void KeyFrameMacroblockHeader::dequantize( const KeyFrameHeader::DerivedQuantities & derived )
{
  /* is macroblock skipped? */
  if ( not has_nonzero_ ) {
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
