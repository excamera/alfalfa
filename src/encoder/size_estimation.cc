/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <utility>

#include "encoder.hh"

using namespace std;

template<>
size_t Encoder::estimate_size<KeyFrame>( const VP8Raster & raster, const size_t y_ac_qi )
{
  auto macroblock_mapper =
    [&]( const unsigned int column, const unsigned int row ) -> pair<unsigned int, unsigned int>
    {
      return { column * WIDTH_SAMPLE_DIMENSION_FACTOR, row * HEIGHT_SAMPLE_DIMENSION_FACTOR };
    };

  DecoderState decoder_state_copy = decoder_state_;
  decoder_state_ = DecoderState( width(), height() );

  KeyFrame & frame = subsampled_key_frame_;

  QuantIndices quant_indices;
  quant_indices.y_ac_qi = y_ac_qi;

  frame.mutable_header().quant_indices = quant_indices;
  frame.mutable_header().refresh_entropy_probs = true; // XXX

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };

  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  update_rd_multipliers( quantizer );

  // TokenBranchCounts token_branch_counts;

  frame.mutable_macroblocks().forall_ij(
    [&] ( KeyFrameMacroblock & frame_mb, unsigned int mb_column, unsigned int mb_row )
    {
      const pair<unsigned int, unsigned int> org_mb_location = macroblock_mapper( mb_column, mb_row );
      const unsigned int org_mb_column = org_mb_location.first;
      const unsigned int org_mb_row = org_mb_location.second;

      auto original_mb = raster.macroblock( org_mb_column, org_mb_row );
      auto reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto temp_mb = temp_raster().macroblock( mb_column, mb_row );

      luma_mb_intra_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                             frame_mb, quantizer, FIRST_PASS );

      chroma_mb_intra_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                                frame_mb, quantizer, FIRST_PASS );

      frame_mb.calculate_has_nonzero();
      frame_mb.reconstruct_intra( quantizer, reconstructed_mb );

      //frame_mb.accumulate_token_branches( token_branch_counts );
    }
  );

  frame.relink_y2_blocks();
  optimize_prob_skip( frame );
  // optimize_probability_tables( frame, token_branch_counts );

  size_t size = frame.serialize( decoder_state_.probability_tables ).size();
  decoder_state_ = decoder_state_copy;

  return size * WIDTH_SAMPLE_DIMENSION_FACTOR * HEIGHT_SAMPLE_DIMENSION_FACTOR;
}

template<>
size_t Encoder::estimate_size<InterFrame>( const VP8Raster & raster, const size_t y_ac_qi )
{
  auto macroblock_mapper =
    [&]( const unsigned int column, const unsigned int row )
    {
      return make_pair( column * WIDTH_SAMPLE_DIMENSION_FACTOR, row * HEIGHT_SAMPLE_DIMENSION_FACTOR );
    };

  InterFrame & frame = subsampled_inter_frame_;

  DecoderState decoder_state_copy = decoder_state_;

  QuantIndices quant_indices;
  quant_indices.y_ac_qi = y_ac_qi;

  frame.mutable_header().quant_indices = quant_indices;
  frame.mutable_header().refresh_entropy_probs = true;
  frame.mutable_header().refresh_last = true;

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { width(), height() };

  MVComponentCounts component_counts;

  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  update_rd_multipliers( quantizer );

  frame.mutable_macroblocks().forall_ij(
  [&] ( InterFrameMacroblock & frame_mb, unsigned int mb_column, unsigned int mb_row )
    {
      const pair<unsigned int, unsigned int> org_mb_location = macroblock_mapper( mb_column, mb_row );
      const unsigned int org_mb_column = org_mb_location.first;
      const unsigned int org_mb_row = org_mb_location.second;

      auto original_mb = raster.macroblock( org_mb_column, org_mb_row );
      auto reconstructed_mb = reconstructed_raster.macroblock( mb_column, mb_row );
      auto temp_mb = temp_raster().macroblock( mb_column, mb_row );

      // Process Y and Y2
      luma_mb_inter_predict( original_mb.macroblock(), reconstructed_mb, temp_mb, frame_mb,
                             quantizer, component_counts,
                             frame.header().quant_indices.y_ac_qi, FIRST_PASS );

      if ( frame_mb.inter_coded() ) {
        chroma_mb_inter_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                                 frame_mb, quantizer, FIRST_PASS );
      }
      else {
        chroma_mb_intra_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                                 frame_mb, quantizer, FIRST_PASS );
      }

      frame_mb.calculate_has_nonzero();

      if ( frame_mb.inter_coded() ) {
        frame_mb.reconstruct_inter( quantizer, references_, reconstructed_mb );
      }
      else {
        frame_mb.reconstruct_intra( quantizer, reconstructed_mb );
      }
    }
  );

  frame.relink_y2_blocks();
  optimize_prob_skip( frame );
  optimize_interframe_probs( frame );

  size_t size = frame.serialize( decoder_state_.probability_tables ).size();
  decoder_state_ = decoder_state_copy;

  return size * WIDTH_SAMPLE_DIMENSION_FACTOR * HEIGHT_SAMPLE_DIMENSION_FACTOR;
}

size_t Encoder::estimate_frame_size( const VP8Raster & raster, const size_t y_ac_qi )
{
  if ( not has_state_ ) {
    return estimate_size<KeyFrame>( raster, y_ac_qi );
  }
  else {
    return estimate_size<InterFrame>( raster, y_ac_qi );
  }
}
