#include <array>
#include <limits>
#include <algorithm>

#include "encoder.hh"
#include "frame_header.hh"

using namespace std;

/*
 * Taken from: libvpx:vp8/encoder/entropy.c:38-42
 */
const uint8_t vp8_coef_bands[ 16 ] =
  { 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7 };

const uint8_t vp8_prev_token_class[ MAX_ENTROPY_TOKENS ] =
  { 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0 };

static unsigned calc_prob( unsigned false_count, unsigned total )
{
  if ( false_count == 0 ) {
    return 0;
  } else {
    return max( 1u, min( 255u, 256 * false_count / total ) );
  }
}

uint8_t token_for_coeff( int16_t coeff )
{
  coeff = abs( coeff );

  switch( coeff ) {
  case 0: return ZERO_TOKEN;
  case 1: return ONE_TOKEN;
  case 2: return TWO_TOKEN;
  case 3: return THREE_TOKEN;
  case 4: return FOUR_TOKEN;
  }

  if ( coeff <=  6 ) return DCT_VAL_CATEGORY1;
  if ( coeff <= 10 ) return DCT_VAL_CATEGORY2;
  if ( coeff <= 18 ) return DCT_VAL_CATEGORY3;
  if ( coeff <= 34 ) return DCT_VAL_CATEGORY4;
  if ( coeff <= 66 ) return DCT_VAL_CATEGORY5;

  return DCT_VAL_CATEGORY6;
}

QuantIndices::QuantIndices()
  : y_ac_qi(), y_dc(), y2_dc(), y2_ac(), uv_dc(), uv_ac()
{}

template<class FrameType>
FrameType Encoder::make_empty_frame( const uint16_t width, const uint16_t height )
{
  BoolDecoder data { { nullptr, 0 } };
  FrameType frame { true, width, height, data };
  frame.parse_macroblock_headers( data, ProbabilityTables {} );
  return frame;
}

template <unsigned int size>
template <class PredictionMode>
void VP8Raster::Block<size>::intra_predict( const PredictionMode mb_mode, TwoD<uint8_t> & output )
{
  TwoDSubRange<uint8_t, size, size> subrange( output, 0, 0 );
  intra_predict( mb_mode, subrange );
}

/* Encoder-specific Macroblock Methods */
void InterFrameMacroblockHeader::set_reference( const reference_frame ref )
{
  is_inter_mb = true;

  switch ( ref ) {
  case CURRENT_FRAME:
    is_inter_mb = false;
    mb_ref_frame_sel1.clear();
    mb_ref_frame_sel2.clear();
    break;

  case LAST_FRAME:
    mb_ref_frame_sel1 = false;
    mb_ref_frame_sel2.clear();
    break;

  case GOLDEN_FRAME:
    mb_ref_frame_sel1 = true;
    mb_ref_frame_sel2 = false;
    break;

  case ALTREF_FRAME:
    mb_ref_frame_sel1 = true;
    mb_ref_frame_sel2 = true;
    break;
  }
}


/* Encoder */
Encoder::Encoder( const string & output_filename, const uint16_t width,
                  const uint16_t height, const bool two_pass )
  : ivf_writer_( output_filename, "VP80", width, height, 1, 1 ),
    width_( width ), height_( height ), temp_raster_handle_( width, height ),
    decoder_state_( width, height ), references_(), costs_(),
    two_pass_encoder_( two_pass )
{
  costs_.fill_mode_costs();
}

template<unsigned int size>
uint32_t Encoder::sse( const VP8Raster::Block<size> & block,
                       const TwoDSubRange<uint8_t, size, size> & prediction )
{
  uint32_t res = 0;

  for ( size_t i = 0; i < size; i++ ) {
    for ( size_t j = 0; j < size; j++ ) {
      int16_t diff = ( block.at( i, j ) - prediction.at( i, j ) );
      res += diff * diff;
    }
  }

  return res;
}

template<unsigned int size>
uint32_t Encoder::variance( const VP8Raster::Block<size> & block,
                            const TwoDSubRange<uint8_t, size, size> & prediction )
{
  uint32_t res = 0;
  int32_t sum = 0;

  for ( size_t i = 0; i < size; i++ ) {
    for ( size_t j = 0; j < size; j++ ) {
      int16_t diff = ( block.at( i, j ) - prediction.at( i, j ) );

      sum += diff;
      res += diff * diff;
    }
  }

  return res - ( ( int64_t)sum * sum ) / ( size * size );
}

template <class MacroblockType>
void Encoder::luma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                     VP8Raster::Macroblock & reconstructed_mb,
                                     VP8Raster::Macroblock & temp_mb,
                                     MacroblockType & frame_mb,
                                     const Quantizer & quantizer,
                                     const EncoderPass encoder_pass ) const
{
  // Select the best prediction mode
  uint32_t min_error = numeric_limits<uint32_t>::max();
  mbmode min_prediction_mode = DC_PRED;

  TwoDSubRange<uint8_t, 16, 16> & prediction = temp_mb.Y.mutable_contents();

  /* Because of the way that reconstructed_mb is used as a buffer to store the
   * best prediction result, it is necessary to first examine the B_PRED and
   * then the other prediction modes. */
  for ( unsigned int prediction_mode = B_PRED; prediction_mode < num_y_modes; prediction_mode-- ) {
    uint32_t error_val = numeric_limits<uint32_t>::max();

    if ( prediction_mode == B_PRED ) {
      error_val = 0;

      uint32_t cost = 0;
      uint32_t distortion = 0;

      reconstructed_mb.Y_sub.forall_ij(
        [&] ( VP8Raster::Block4 & reconstructed_sb, unsigned int sb_column, unsigned int sb_row )
        {
          auto & original_sb = original_mb.Y_sub.at( sb_column, sb_row );
          auto & temp_sb = temp_mb.Y_sub.at( sb_column, sb_row );
          auto & frame_sb = frame_mb.Y().at( sb_column, sb_row );

          const auto above_mode = frame_sb.context().above.initialized()
            ? frame_sb.context().above.get()->prediction_mode() : B_DC_PRED;
          const auto left_mode = frame_sb.context().left.initialized()
            ? frame_sb.context().left.get()->prediction_mode() : B_DC_PRED;

          bmode sb_prediction_mode = luma_sb_intra_predict( original_sb,
            reconstructed_sb, temp_sb, costs_.bmode_costs.at( above_mode ).at( left_mode ) );

          distortion += sse( original_sb, reconstructed_sb.contents() );

          frame_sb.mutable_coefficients().subtract_dct( original_sb,
            reconstructed_sb.contents() );

          if ( encoder_pass == FIRST_PASS ) {
            frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
          }
          else {
            trellis_quantize( frame_sb, quantizer );
          }

          frame_sb.set_prediction_mode( sb_prediction_mode );
          frame_sb.set_Y_without_Y2();
          frame_sb.calculate_has_nonzero();

          reconstructed_sb.intra_predict( sb_prediction_mode );
          frame_sb.dequantize( quantizer ).idct_add( reconstructed_sb );

          cost += costs_.bmode_costs.at( above_mode ).at( left_mode ).at( sb_prediction_mode );
        }
      );

      error_val = rdcost( cost, distortion,
                          RATE_MULTIPLIER, DISTORTION_MULTIPLIER );
    }
    else {
      reconstructed_mb.Y.intra_predict( ( mbmode )prediction_mode, prediction );

      /* Here we compute variance, instead of SSE, because in this case
       * the average will be taken out from Y2 block into the Y2 block. */
      uint32_t distortion = variance( original_mb.Y, prediction );

      uint16_t bit_cost = costs_.mbmode_costs.at( 0 /* keyframe */ ).at( prediction_mode );
      error_val = rdcost( bit_cost, distortion, RATE_MULTIPLIER,
                          DISTORTION_MULTIPLIER );
    }

    if ( error_val < min_error ) {
      reconstructed_mb.Y.mutable_contents().copy_from( prediction );
      min_prediction_mode = ( mbmode )prediction_mode;
      min_error = error_val;
    }
  }

  // Apply
  frame_mb.Y2().set_prediction_mode( min_prediction_mode );

  if ( min_prediction_mode != B_PRED ) { // if B_PRED is selected, it is already taken care of.
    SafeArray<int16_t, 16> walsh_input;

    frame_mb.Y().forall_ij(
      [&] ( YBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
      {
        auto & original_sb = original_mb.Y_sub.at( sb_column, sb_row );
        frame_sb.set_prediction_mode( KeyFrameMacroblock::implied_subblock_mode( min_prediction_mode ) );

        frame_sb.mutable_coefficients().subtract_dct( original_sb,
          reconstructed_mb.Y_sub.at( sb_column, sb_row ).contents() );

        walsh_input.at( sb_column + 4 * sb_row ) = frame_sb.coefficients().at( 0 );
        frame_sb.set_dc_coefficient( 0 );
        frame_sb.set_Y_after_Y2();

        if ( encoder_pass == FIRST_PASS ) {
          frame_sb.mutable_coefficients() = YBlock::quantize( quantizer, frame_sb.coefficients() );
        }
        else {
          trellis_quantize( frame_sb, quantizer );
        }

        frame_sb.calculate_has_nonzero();
      }
    );

    frame_mb.Y2().set_coded( true );
    frame_mb.Y2().mutable_coefficients().wht( walsh_input );

    if ( encoder_pass == FIRST_PASS ) {
      frame_mb.Y2().mutable_coefficients() = Y2Block::quantize( quantizer, frame_mb.Y2().coefficients() );
    }
    else {
      check_reset_y2( frame_mb.Y2(), quantizer );
      trellis_quantize( frame_mb.Y2(), quantizer );
    }

    frame_mb.Y2().calculate_has_nonzero();
  }
  else {
    frame_mb.Y2().set_coded( false );
  }
}

/*
 * Based on libvpx:vp8/encoder/encodemb.c:413
 */
void Encoder::check_reset_y2( Y2Block & y2, const Quantizer & quantizer ) const
{
  int sum = 0;

  if ( quantizer.y2_dc >= 35 and quantizer.y2_ac >= 35 ) {
    return;
  }

  for ( size_t i = 0; i < 16; i++ ) {
    sum += abs( y2.coefficients().at( i ) );

    if ( sum >= 35 ) {
      return;
    }
  }

  // sum < 35
  for ( int i = 0; i < 16; i++ ) {
    y2.mutable_coefficients().at( i ) = 0;
  }
}

template <class MacroblockType>
void Encoder::chroma_mb_intra_predict( const VP8Raster::Macroblock & original_mb,
                                       VP8Raster::Macroblock & reconstructed_mb,
                                       VP8Raster::Macroblock & temp_mb,
                                       MacroblockType & frame_mb,
                                       const Quantizer & quantizer,
                                       const EncoderPass encoder_pass ) const
{
  // Select the best prediction mode
  uint32_t min_error = numeric_limits<uint32_t>::max();
  mbmode min_prediction_mode = DC_PRED;

  TwoDSubRange<uint8_t, 8, 8> & u_prediction = temp_mb.U.mutable_contents();
  TwoDSubRange<uint8_t, 8, 8> & v_prediction = temp_mb.V.mutable_contents();

  for ( unsigned int prediction_mode = 0; prediction_mode < num_uv_modes; prediction_mode++ ) {
    reconstructed_mb.U.intra_predict( ( mbmode )prediction_mode, u_prediction );
    reconstructed_mb.V.intra_predict( ( mbmode )prediction_mode, v_prediction );

    uint32_t error_val = sse( original_mb.U, u_prediction )
                       + sse( original_mb.V, v_prediction );

    if ( error_val < min_error ) {
      reconstructed_mb.U.mutable_contents().copy_from( u_prediction );
      reconstructed_mb.V.mutable_contents().copy_from( v_prediction );
      min_prediction_mode = ( mbmode )prediction_mode;
      min_error = error_val;
    }
  }

  // Apply
  frame_mb.U().at( 0, 0 ).set_prediction_mode( min_prediction_mode );

  frame_mb.U().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.U_sub.at( sb_column, sb_row );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.U_sub.at( sb_column, sb_row ).contents() );

        if ( encoder_pass == FIRST_PASS ) {
          frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
        }
        else {
          trellis_quantize( frame_sb, quantizer );
        }

      frame_sb.calculate_has_nonzero();
    }
  );

  frame_mb.V().forall_ij(
    [&] ( UVBlock & frame_sb, unsigned int sb_column, unsigned int sb_row )
    {
      auto & original_sb = original_mb.V_sub.at( sb_column, sb_row );

      frame_sb.mutable_coefficients().subtract_dct( original_sb,
        reconstructed_mb.V_sub.at( sb_column, sb_row ).contents() );

      if ( encoder_pass == FIRST_PASS ) {
        frame_sb.mutable_coefficients() = UVBlock::quantize( quantizer, frame_sb.coefficients() );
      }
      else {
        trellis_quantize( frame_sb, quantizer );
      }

      frame_sb.calculate_has_nonzero();
    }
  );
}

/* This function outputs the prediction values to 'reconstructed_sb'
 * and returns the prediction mode.
 */
bmode Encoder::luma_sb_intra_predict( const VP8Raster::Block4 & original_sb,
                                      VP8Raster::Block4 & reconstructed_sb,
                                      VP8Raster::Block4 & temp_sb,
                                      const SafeArray<uint16_t, num_intra_b_modes> & mode_costs ) const
{
  uint32_t min_error = numeric_limits<uint32_t>::max();
  bmode min_prediction_mode = B_DC_PRED;
  TwoDSubRange<uint8_t, 4, 4> & prediction = temp_sb.mutable_contents();

  for ( unsigned int prediction_mode = 0; prediction_mode < num_intra_b_modes; prediction_mode++ ) {
    reconstructed_sb.intra_predict( ( bmode )prediction_mode, prediction );

    uint32_t distortion = sse( original_sb, prediction );
    uint32_t error_val = rdcost( mode_costs.at( prediction_mode ), distortion,
                                 RATE_MULTIPLIER, DISTORTION_MULTIPLIER );

    if ( error_val < min_error ) {
      reconstructed_sb.mutable_contents().copy_from( prediction );
      min_prediction_mode = ( bmode )prediction_mode;
      min_error = error_val;
    }
  }

  return min_prediction_mode;
}

template<class FrameSubblockType>
void Encoder::trellis_quantize( FrameSubblockType & frame_sb,
                                const Quantizer & quantizer ) const
{
  struct TrellisNode
  {
    uint32_t rate;
    uint32_t distortion;
    uint32_t cost;
    int16_t coeff;

    uint8_t token;
    uint8_t next;
  };

  uint16_t dc_factor;
  uint16_t ac_factor;

  // select quantization factors based on macroblock type
  switch( frame_sb.type() ) {
  case BlockType::UV:
    dc_factor = quantizer.uv_dc;
    ac_factor = quantizer.uv_ac;
    break;

  case BlockType::Y2:
    dc_factor = quantizer.y2_dc;
    ac_factor = quantizer.y2_ac;
    break;

  default:
    dc_factor = quantizer.y_dc;
    ac_factor = quantizer.y_ac;
  }

  const size_t first_index = ( frame_sb.type() == BlockType::Y_after_Y2 ) ? 1 : 0;
  uint8_t coded_length = 0;

  /* how many tokens are we going to encode? */
  for ( size_t index = first_index; index < 16; index++ ) {
    if ( frame_sb.coefficients().at( zigzag.at( index ) ) ) {
      coded_length = index + 1;
    }
  }

  if ( coded_length == 0 ) {
    // everything is zero. our work is done here.
    for ( size_t i = 0; i < 16; i++ ) {
      frame_sb.mutable_coefficients().at( i ) = 0;
    }

    return;
  }

  const uint8_t LEVELS = 2;
  SafeArray<SafeArray<TrellisNode, LEVELS>, 17> trellis;

  // setting up the sentinel node for the trellis
  for ( size_t i = 0; i < LEVELS; i++ ) {
    TrellisNode & sentinel_node = trellis.at( coded_length ).at( i );
    sentinel_node.rate = 0;
    sentinel_node.distortion = 0;
    sentinel_node.token = DCT_EOB_TOKEN;
    sentinel_node.coeff = 0;
    sentinel_node.next = numeric_limits<uint8_t>::max();
  }

  // building the trellis first
  for ( uint8_t idx = coded_length; idx-- > first_index; ) {
    const int16_t original_coeff = frame_sb.coefficients().at( zigzag.at( idx ) );
    const int16_t quantized_coeff = original_coeff /
                                    ( idx == 0 ? dc_factor : ac_factor );

    // evaluate two quantizer levels: {q, q - 1}
    // it's possible to explore more
    for ( size_t q_shift = 0; q_shift < LEVELS; q_shift++ ) {
      TrellisNode & current_node = trellis.at( idx ).at( q_shift );

      int16_t candidate_coeff = quantized_coeff;

      if ( candidate_coeff < 0 ) {
        candidate_coeff += q_shift;
        candidate_coeff = min( ( int16_t )0, candidate_coeff );
      }
      else if ( candidate_coeff > 0 or q_shift == 0 )  {
        candidate_coeff -= q_shift;
        candidate_coeff = max( ( int16_t )0, candidate_coeff );
      }
      else { // candidate_coeff == 0 and q_shift != 0
        current_node = trellis.at( idx ).at( q_shift - 1 );
        continue;
      }

      int16_t diff = ( original_coeff - candidate_coeff * ( idx == 0 ? dc_factor : ac_factor ) );
      uint32_t sse = diff * diff;

      current_node.coeff = candidate_coeff;
      current_node.token = token_for_coeff( candidate_coeff );

      array<uint32_t, LEVELS> distortions;
      array<uint32_t, LEVELS> rates;
      array<uint32_t, LEVELS> rd_costs;

      // fill the trellis for current coeff index and select the best
      uint8_t best_next = numeric_limits<uint8_t>::max();
      uint32_t best_cost = numeric_limits<uint32_t>::max();

      for ( size_t next = 0; next < LEVELS; next++ ) {
        const TrellisNode & next_node = trellis.at( idx + 1 ).at( next );

        distortions[ next ] = trellis.at( idx + 1 ).at( next ).distortion + sse;
              rates[ next ] = trellis.at( idx + 1 ).at( next ).rate;

        if ( idx < 15 ) {
          size_t next_band = vp8_coef_bands[ idx + 1 ];
          size_t current_context = vp8_prev_token_class[ current_node.token ];

          // cost of the next token based on the *current* context
          rates[ next ] += costs_.token_costs.at( frame_sb.type() )
                                             .at( next_band )
                                             .at( current_context )
                                             .at( next_node.token );
        }

        rd_costs[ next ] = rdcost( rates[ next ], distortions[ next ],
                                   RATE_MULTIPLIER, DISTORTION_MULTIPLIER );

        if ( rd_costs[ next ] < best_cost ) {
          best_cost = rd_costs[ next ];
          best_next = next;
        }
      }

      if ( current_node.coeff != 0 or trellis.at( idx + 1 ).at( best_next ).token != DCT_EOB_TOKEN ) {
        current_node.rate = rates[ best_next ] + Costs::coeff_base_cost( current_node.coeff );
        current_node.distortion = distortions[ best_next ];
        current_node.cost = rd_costs[ best_next ];
        current_node.next = best_next;
      }
      else {
        // this token is zero and the next one is EOB, so let's move EOB back here
        current_node.coeff = 0;
        current_node.rate = 0;
        current_node.distortion = sse;
        current_node.cost = rdcost( 0, sse, RATE_MULTIPLIER, DISTORTION_MULTIPLIER);
        current_node.next = numeric_limits<uint8_t>::max();
        current_node.token = DCT_EOB_TOKEN;
      }
    }
  }

  for ( size_t i = 0; i < LEVELS; i++ ) {
    TrellisNode & node = trellis.at( first_index ).at( i );
    node.rate += costs_.token_costs.at( frame_sb.type() )
                                   .at( vp8_coef_bands[ first_index ] )
                                   .at( 0 )
                                   .at( node.token );

    node.cost = rdcost( node.rate, node.distortion, RATE_MULTIPLIER,
                        DISTORTION_MULTIPLIER );
  }

  // walking the minium path through trellis
  uint32_t min_cost = numeric_limits<uint32_t>::max();
  size_t min_choice;
  for ( size_t i = 0; i < LEVELS; i++ ) {
    if ( trellis.at( first_index ).at( i ).cost < min_cost ) {
      min_cost = trellis.at( first_index ).at( i ).cost;
      min_choice = i;
    }
  }

  size_t i;
  for ( i = first_index; i < 16; i++ ) {
    if ( trellis.at( i ).at( min_choice ).token == DCT_EOB_TOKEN ) {
      break;
    }

    frame_sb.mutable_coefficients().at( zigzag.at( i ) ) = trellis.at( i ).at( min_choice ).coeff;
    min_choice = trellis.at( i ).at( min_choice ).next;
  }

  for ( ; i < 16; i++ ) {
    frame_sb.mutable_coefficients().at( zigzag.at( i ) ) = 0;
  }
}

uint32_t Encoder::rdcost( uint32_t rate, uint32_t distortion,
                          uint32_t rate_multiplier,
                          uint32_t distortion_multiplier )
{
  return ( ( 128 + rate * rate_multiplier ) / 256 )
         + distortion * distortion_multiplier;
}

template<class FrameType>
void Encoder::optimize_probability_tables( FrameType & frame, const TokenBranchCounts & token_branch_counts )
{
  for ( unsigned int i = 0; i < BLOCK_TYPES; i++ ) {
    for ( unsigned int j = 0; j < COEF_BANDS; j++ ) {
      for ( unsigned int k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
        for ( unsigned int l = 0; l < ENTROPY_NODES; l++ ) {
          const unsigned int false_count = token_branch_counts.at( i ).at( j ).at( k ).at( l ).first;
          const unsigned int true_count = token_branch_counts.at( i ).at( j ).at( k ).at( l ).second;

          const unsigned int prob = calc_prob( false_count, false_count + true_count );

          assert( prob <= 255 );

          if ( prob > 0 and prob != k_default_coeff_probs.at( i ).at( j ).at( k ).at( l ) ) {
            frame.mutable_header().token_prob_update.at( i ).at( j ).at( k ).at( l ) = TokenProbUpdate( true, prob );
          }
        }
      }
    }
  }

  decoder_state_.probability_tables.coeff_prob_update( frame.header() );
}

template<>
pair<InterFrame, double> Encoder::encode_with_quantizer<InterFrame>( const VP8Raster & raster,
                                                                     const QuantIndices & quant_indices )
{
  const uint16_t width = raster.display_width();
  const uint16_t height = raster.display_height();

  InterFrame frame = Encoder::make_empty_frame<InterFrame>( width, height );
  frame.mutable_header().quant_indices = quant_indices;

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width, height };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  raster.macroblocks().forall_ij(
    [&] ( VP8Raster::Macroblock & original_mb, unsigned int mb_column, unsigned int mb_row )
    {
      auto & reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto & temp_mb = temp_raster().macroblock( mb_column, mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_intra_predict( original_mb, reconstructed_mb, temp_mb, frame_mb, quantizer, FIRST_PASS );
      chroma_mb_intra_predict( original_mb, reconstructed_mb, temp_mb, frame_mb, quantizer, FIRST_PASS );

      frame_mb.mutable_header().is_inter_mb = false;
      frame_mb.mutable_header().set_reference( LAST_FRAME );

      frame.relink_y2_blocks();
      frame_mb.calculate_has_nonzero();
      frame_mb.reconstruct_intra( quantizer, reconstructed_mb );
    }
  );

  return make_pair( move( frame ), reconstructed_raster.quality( raster ) );
}

template<>
pair<KeyFrame, double> Encoder::encode_with_quantizer<KeyFrame>( const VP8Raster & raster,
                                                                 const QuantIndices & quant_indices )
{
  const uint16_t width = raster.display_width();
  const uint16_t height = raster.display_height();

  KeyFrame frame = Encoder::make_empty_frame<KeyFrame>( width, height );
  frame.mutable_header().quant_indices = quant_indices;

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width, height };
  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  /* This is how VP8 sets the coefficients for rd-cost.
   * libvpx:vp8/encoder/rdopt.c:270
   */
  double q_ac = ( quantizer.y_ac < 160 ) ? quantizer.y_ac : 160.0 ;
  RATE_MULTIPLIER = q_ac * q_ac * 2.80;

  if ( RATE_MULTIPLIER > 1000 ) {
    DISTORTION_MULTIPLIER = 1;
    RATE_MULTIPLIER /= 100;
  }
  else {
    DISTORTION_MULTIPLIER = 100;
  }

  TokenBranchCounts token_branch_counts;

  for ( size_t pass = FIRST_PASS;
        pass <= ( two_pass_encoder_? SECOND_PASS : FIRST_PASS );
        pass++ ) {

    if ( pass == SECOND_PASS ) {
      costs_.fill_token_costs( decoder_state_.probability_tables );
      token_branch_counts = TokenBranchCounts();
    }

    raster.macroblocks().forall_ij(
      [&] ( VP8Raster::Macroblock & original_mb, unsigned int mb_column, unsigned int mb_row )
      {
        auto & reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
        auto & temp_mb = temp_raster().macroblock( mb_column, mb_row );
        auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

        // Process Y and Y2
        luma_mb_intra_predict( original_mb, reconstructed_mb, temp_mb, frame_mb, quantizer, FIRST_PASS );
        chroma_mb_intra_predict( original_mb, reconstructed_mb, temp_mb, frame_mb, quantizer, FIRST_PASS );

        frame.relink_y2_blocks();
        frame_mb.calculate_has_nonzero();
        frame_mb.reconstruct_intra( quantizer, reconstructed_mb );

        frame_mb.accumulate_token_branches( token_branch_counts );
      }
    );

    optimize_probability_tables( frame, token_branch_counts );
  }

  frame.mutable_header().mode_lf_adjustments.initialize();
  frame.mutable_header().mode_lf_adjustments.get().initialize();

  for ( size_t i = 0; i < 4; i++ ) {
    frame.mutable_header().mode_lf_adjustments.get().get().ref_update.at( i ).initialize( 0 );
    frame.mutable_header().mode_lf_adjustments.get().get().mode_update.at( i ).initialize( 0 );
  }

  size_t best_lf_level = 0;
  double best_ssim = -1.0;

  for ( size_t lf_level = 0; lf_level < 64; lf_level++ ) {
    temp_raster().copy_from( reconstructed_raster );

    frame.mutable_header().loop_filter_level = lf_level;

    decoder_state_.filter_adjustments.clear();
    decoder_state_.filter_adjustments.initialize( frame.header() );

    frame.loopfilter( decoder_state_.segmentation, decoder_state_.filter_adjustments, temp_raster() );

    double ssim = temp_raster().quality( raster );

    if ( ssim > best_ssim ) {
      best_ssim = ssim;
      best_lf_level = lf_level;
    }
    else {
      break;
    }
  }

  frame.mutable_header().loop_filter_level = best_lf_level;
  decoder_state_.filter_adjustments.clear();
  decoder_state_.filter_adjustments.initialize( frame.header() );

  frame.loopfilter( decoder_state_.segmentation, decoder_state_.filter_adjustments, reconstructed_raster );
  return make_pair( move( frame ), reconstructed_raster.quality( raster ) );
}

template<class FrameType>
double Encoder::encode_raster( const VP8Raster & raster,
                               const double minimum_ssim,
                               const uint8_t y_ac_qi )
{
  int y_ac_qi_min = 0;
  int y_ac_qi_max = 127;

  if ( y_ac_qi != numeric_limits<uint8_t>::max() ) {
    if ( y_ac_qi > 127 ) {
      throw runtime_error( "y_ac_qi should be less than or equal to 127" );
    }

    y_ac_qi_min = y_ac_qi;
    y_ac_qi_max = y_ac_qi;
  }

  const uint16_t width = raster.display_width();
  const uint16_t height = raster.display_height();

  if ( width != width_ or height != height_ ) {
    throw runtime_error( "scaling is not supported." );
  }

  QuantIndices quant_indices;

  bool found = false;
  size_t best_y_ac_qi = 0;

  while ( y_ac_qi_min <= y_ac_qi_max ) {
    decoder_state_ = DecoderState( width_, height_ );

    quant_indices.y_ac_qi = ( y_ac_qi_min + y_ac_qi_max ) / 2;
    pair<FrameType, double> encoded_frame = encode_with_quantizer<FrameType>( raster, quant_indices );

    double current_ssim = encoded_frame.second;

    if ( current_ssim >= minimum_ssim || ( y_ac_qi_min == y_ac_qi_max && not found ) ) {
      // this is a potential answer, let's save it
      found = true;
      best_y_ac_qi = quant_indices.y_ac_qi;
    }

    if ( y_ac_qi_min == y_ac_qi_max ) {
      break; // "I believe the search is over."
    }

    if ( current_ssim < minimum_ssim ) {
      y_ac_qi_max = quant_indices.y_ac_qi - 1;
    }
    else {
      y_ac_qi_min = quant_indices.y_ac_qi + 1;
    }
  }

  quant_indices.y_ac_qi = best_y_ac_qi;
  decoder_state_ = DecoderState( width_, height_ );
  pair<FrameType, double> encoded_frame = encode_with_quantizer<FrameType>( raster, quant_indices );

  ivf_writer_.append_frame( encoded_frame.first.serialize( decoder_state_.probability_tables ) );
  return encoded_frame.second;
}

bool Encoder::should_encode_as_keyframe( const VP8Raster & )
{
  return not ( references_.last.initialized()
               or references_.golden.initialized()
               or references_.alternative_reference.initialized() );
}

double Encoder::encode( const VP8Raster & raster, const double minimum_ssim,
                        const uint8_t y_ac_qi )
{
  if ( should_encode_as_keyframe( raster ) ) {
    references_.clear_all();
    return encode_raster<KeyFrame>( raster, minimum_ssim, y_ac_qi );
  }
  else {
    return encode_raster<InterFrame>( raster, minimum_ssim, y_ac_qi );
  }
}
