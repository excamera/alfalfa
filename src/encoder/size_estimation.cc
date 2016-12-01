/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <utility>

#include "encoder.hh"

using namespace std;

size_t Encoder::estimate_keyframe_size( const VP8Raster & raster, const size_t y_ac_qi )
{
  const size_t SAMPLE_DIMENSION_FACTOR = 4;

  const unsigned int sample_width = raster.width() / SAMPLE_DIMENSION_FACTOR;
  const unsigned int sample_height = raster.height() / SAMPLE_DIMENSION_FACTOR;

  DecoderState decoder_state_copy = decoder_state_;
  decoder_state_ = DecoderState( width(), height() );

  auto macroblock_mapper =
    [&]( const unsigned int column, const unsigned int row )
    {
      return make_pair( column * SAMPLE_DIMENSION_FACTOR, row * SAMPLE_DIMENSION_FACTOR );
    };

  KeyFrame frame = make_empty_frame<KeyFrame>( sample_width, sample_height, true );

  QuantIndices quant_indices;
  quant_indices.y_ac_qi = y_ac_qi;

  frame.mutable_header().quant_indices = quant_indices;
  frame.mutable_header().refresh_entropy_probs = true; // XXX

  Quantizer quantizer( frame.header().quant_indices );
  MutableRasterHandle reconstructed_raster_handle { sample_width, sample_height };

  VP8Raster & reconstructed_raster = reconstructed_raster_handle.get();

  update_rd_multipliers( quantizer );

  reconstructed_raster.macroblocks_forall_ij(
    [&] ( VP8Raster::Macroblock reconstructed_mb, unsigned int mb_column, unsigned int mb_row )
    {
      const pair<unsigned int, unsigned int> org_mb_location = macroblock_mapper( mb_column, mb_row );
      const unsigned int org_mb_column = org_mb_location.first;
      const unsigned int org_mb_row = org_mb_location.second;

      auto original_mb = raster.macroblock( org_mb_column, org_mb_row );
      auto temp_mb = temp_raster().macroblock( org_mb_column, org_mb_row );
      auto & frame_mb = frame.mutable_macroblocks().at( mb_column, mb_row );

      luma_mb_intra_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                             frame_mb, quantizer, FIRST_PASS );

      chroma_mb_intra_predict( original_mb.macroblock(), reconstructed_mb, temp_mb,
                                frame_mb, quantizer, FIRST_PASS );

      frame_mb.calculate_has_nonzero();
      frame_mb.reconstruct_intra( quantizer, reconstructed_mb );
    }
  );

  size_t size = frame.serialize( decoder_state_.probability_tables ).size();
  decoder_state_ = decoder_state_copy;

  return size * SAMPLE_DIMENSION_FACTOR * SAMPLE_DIMENSION_FACTOR;
}
