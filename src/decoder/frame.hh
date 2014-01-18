#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock.hh"
#include "uncompressed_chunk.hh"
#include "modemv_data.hh"
#include "raster.hh"

struct References;

struct Quantizers
{
  Quantizer quantizer;
  SafeArray< Quantizer, num_segments > segment_quantizers;
};

template <class FrameHeaderType, class MacroblockType>
class Frame
{
 private:
  bool show_;

  unsigned int display_width_, display_height_;
  unsigned int macroblock_width_ { Raster::macroblock_dimension( display_width_ ) },
    macroblock_height_ { Raster::macroblock_dimension( display_height_ ) };

  TwoD< Y2Block > Y2_ { macroblock_width_, macroblock_height_ };
  TwoD< YBlock > Y_ { 4 * macroblock_width_, 4 * macroblock_height_ };
  TwoD< UVBlock > U_ { 2 * macroblock_width_, 2 * macroblock_height_ };
  TwoD< UVBlock > V_ { 2 * macroblock_width_, 2 * macroblock_height_ };

  BoolDecoder first_partition_;
  FrameHeaderType header_ { first_partition_ };

  std::vector< BoolDecoder > dct_partitions_;

  Optional< TwoD< MacroblockType > > macroblock_headers_ {};

  void relink_y2_blocks( void );

  void loopfilter( const QuantizerFilterAdjustments & quantizer_filter_adjustments, Raster & target ) const;

  ProbabilityArray< num_segments > calculate_mb_segment_tree_probs( void ) const;
  SafeArray< Quantizer, num_segments > calculate_segment_quantizers( const QuantizerFilterAdjustments & quantizer_filter_adjustments ) const;

 public:
  Frame( const UncompressedChunk & chunk,
	 const unsigned int width,
	 const unsigned int height );

  const FrameHeaderType & header( void ) const { return header_; }

  void parse_macroblock_headers_and_update_segmentation_map( SegmentationMap & segmentation_map,
							     const ProbabilityTables & probability_tables );
  void parse_tokens( const ProbabilityTables & probability_tables );

  void decode( const QuantizerFilterAdjustments & quantizer_filter_adjustments, Raster & raster ) const;
  void decode( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
	       const References & references, Raster & raster ) const;

  void copy_to( const RasterHandle & raster, References & references ) const;

  std::vector< uint8_t > serialize_first_partition( const ProbabilityTables & probability_tables ) const;
  std::vector< std::vector< uint8_t > > serialize_tokens( const ProbabilityTables & probability_tables ) const;
};

using KeyFrame = Frame< KeyFrameHeader, KeyFrameMacroblock >;
using InterFrame = Frame< InterFrameHeader, InterFrameMacroblock >;

#endif /* FRAME_HH */
