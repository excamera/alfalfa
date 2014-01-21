#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock.hh"
#include "modemv_data.hh"
#include "raster.hh"

struct References;
struct QuantizerFilterAdjustments;

struct Quantizers
{
  Quantizer quantizer;
  SafeArray< Quantizer, num_segments > segment_quantizers;
};

struct ContinuationHeader
{
  Flag missing_last_frame;
  Flag missing_golden_frame;
  Flag missing_alternate_reference_frame;

  ContinuationHeader( BoolDecoder & data )
    : missing_last_frame( data ),
      missing_golden_frame( data ),
      missing_alternate_reference_frame( data )
  {}

  bool is_missing( const reference_frame reference_id ) const;
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

  void loopfilter( const QuantizerFilterAdjustments & quantizer_filter_adjustments, Raster & target ) const;

  ProbabilityArray< num_segments > calculate_mb_segment_tree_probs( void ) const;
  SafeArray< Quantizer, num_segments > calculate_segment_quantizers( const QuantizerFilterAdjustments & quantizer_filter_adjustments ) const;

  std::vector< uint8_t > serialize_first_partition( const ProbabilityTables & probability_tables ) const;
  std::vector< std::vector< uint8_t > > serialize_tokens( const ProbabilityTables & probability_tables ) const;

 public:
  Frame( const bool show,
	 const bool continuation,
	 const unsigned int width,
	 const unsigned int height,
	 BoolDecoder & first_partition );

  const FrameHeaderType & header( void ) const { return header_; }

  void parse_macroblock_headers_and_update_segmentation_map( BoolDecoder & rest_of_first_partition,
							     SegmentationMap & segmentation_map,
							     const ProbabilityTables & probability_tables );
  void parse_tokens( std::vector< Chunk > dct_partitions, const ProbabilityTables & probability_tables );

  void decode( const QuantizerFilterAdjustments & quantizer_filter_adjustments, Raster & raster ) const;
  void decode( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
	       const References & references, Raster & raster ) const;

  void copy_to( const RasterHandle & raster, References & references ) const;

  std::vector< uint8_t > serialize( const ProbabilityTables & probability_tables ) const;

  uint8_t dct_partition_count( void ) const { return 1 << header_.log2_number_of_dct_partitions; }

  bool show_frame( void ) const { return show_; }

  void rewrite_as_intra( const QuantizerFilterAdjustments & quantizer_filter_adjustments,
			 const References & references, Raster & raster );
};

using KeyFrame = Frame< KeyFrameHeader, KeyFrameMacroblock >;
using InterFrame = Frame< InterFrameHeader, InterFrameMacroblock >;

#endif /* FRAME_HH */
