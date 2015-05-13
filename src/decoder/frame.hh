#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock.hh"

struct References;
struct Segmentation;
struct FilterAdjustments;

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

  FrameHeaderType header_;

  Optional< ContinuationHeader > continuation_header_;

  Optional< TwoD< MacroblockType > > macroblock_headers_ {};

  void relink_y2_blocks( void );

  ProbabilityArray< num_segments > calculate_mb_segment_tree_probs( void ) const;
  SafeArray< Quantizer, num_segments > calculate_segment_quantizers( const Optional< Segmentation > & segmentation ) const;

  std::vector< uint8_t > serialize_first_partition( const ProbabilityTables & probability_tables ) const;
  std::vector< std::vector< uint8_t > > serialize_tokens( const ProbabilityTables & probability_tables ) const;

 public:
  void loopfilter( const Optional< Segmentation > & segmentation,
		   const Optional< FilterAdjustments > & quantizer_filter_adjustments,
		   Raster & target ) const;

  Frame( const bool show,
	 const bool continuation,
	 const unsigned int width,
	 const unsigned int height,
	 BoolDecoder & first_partition );

  // Diff Constructor
  Frame( const Frame & original,
	 const DecoderState & source_decoder_state,
	 const DecoderState & target_decoder_state,
	 const References & source_references,
	 const References & target_references );

  const FrameHeaderType & header( void ) const { return header_; }
  const Optional< ContinuationHeader > & continuation_header( void ) const { return continuation_header_; }

  std::array<bool, 4> used_references( void ) const;

  void parse_macroblock_headers( BoolDecoder & rest_of_first_partition,
				 const ProbabilityTables & probability_tables );

  void update_segmentation( SegmentationMap & mutable_segmentation_map );

  void parse_tokens( std::vector< Chunk > dct_partitions, const ProbabilityTables & probability_tables );

  void decode( const Optional< Segmentation > & segmentation,
	       Raster & raster ) const;

  void decode( const Optional< Segmentation > & segmentation,
	       const References & references, Raster & raster ) const;

  void copy_to( const RasterHandle & raster, References & references ) const;

  void optimize_continuation_coefficients( void );

  std::vector< uint8_t > serialize( const ProbabilityTables & probability_tables ) const;

  uint8_t dct_partition_count( void ) const { return 1 << header_.log2_number_of_dct_partitions; }

  bool show_frame( void ) const { return show_; }
};

using KeyFrame = Frame< KeyFrameHeader, KeyFrameMacroblock >;
using InterFrame = Frame< InterFrameHeader, InterFrameMacroblock >;

#endif /* FRAME_HH */
