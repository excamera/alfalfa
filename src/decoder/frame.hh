#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock.hh"
#include "dependency_tracking.hh"

struct References;
struct Segmentation;
struct FilterAdjustments;

class ReferenceDependency;

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
  unsigned int macroblock_width_ { VP8Raster::macroblock_dimension( display_width_ ) },
    macroblock_height_ { VP8Raster::macroblock_dimension( display_height_ ) };

  TwoD<Y2Block> Y2_ { macroblock_width_, macroblock_height_ };
  TwoD<YBlock> Y_ { 4 * macroblock_width_, 4 * macroblock_height_ };
  TwoD<UVBlock> U_ { 2 * macroblock_width_, 2 * macroblock_height_ };
  TwoD<UVBlock> V_ { 2 * macroblock_width_, 2 * macroblock_height_ };

  FrameHeaderType header_;

  Optional<TwoD<MacroblockType>> macroblock_headers_ {};

  UpdateTracker ref_updates_;

  void relink_y2_blocks( void );

  ProbabilityArray< num_segments > calculate_mb_segment_tree_probs( void ) const;
  SafeArray< Quantizer, num_segments > calculate_segment_quantizers( const Optional< Segmentation > & segmentation ) const;

  std::vector< uint8_t > serialize_first_partition( const ProbabilityTables & probability_tables ) const;
  std::vector< std::vector< uint8_t > > serialize_tokens( const ProbabilityTables & probability_tables ) const;

 public:
  void loopfilter( const Optional< Segmentation > & segmentation,
		   const Optional< FilterAdjustments > & quantizer_filter_adjustments,
		   VP8Raster & target ) const;

  Frame( const bool show,
	 const unsigned int width,
	 const unsigned int height,
	 BoolDecoder & first_partition );

  /* Construct a StateUpdateFrame */
  Frame( const ProbabilityTables & source_probs,
         const ProbabilityTables & target_probs );

  /* Construct a RefUpdateFrame */
  Frame( const reference_frame & ref_to_update, const ReferenceUpdater & update_info );

  /* Rewrite InterFrame */
  Frame( const Frame & original,
         const Optional<Segmentation> & target_segmentation,
         const Optional<FilterAdjustments> & target_filter );

  const FrameHeaderType & header() const { return header_; }

  DependencyTracker get_used() const;
  UpdateTracker get_updated() const { return ref_updates_; }

  const TwoD<MacroblockType> & macroblocks() const { return macroblock_headers_.get(); }

  void parse_macroblock_headers( BoolDecoder & rest_of_first_partition,
				 const ProbabilityTables & probability_tables );

  void update_segmentation( SegmentationMap & mutable_segmentation_map );

  void parse_tokens( std::vector< Chunk > dct_partitions, const ProbabilityTables & probability_tables );

  void decode( const Optional< Segmentation > & segmentation, const References & references,
               VP8Raster & raster ) const;

  void copy_to( const RasterHandle & raster, References & references ) const;

  std::string reference_update_stats( void ) const;

  std::string stats( void ) const;

  void optimize_continuation_coefficients( void );

  std::vector< uint8_t > serialize( const ProbabilityTables & probability_tables ) const;

  uint8_t dct_partition_count( void ) const { return 1 << header_.log2_number_of_dct_partitions; }

  bool show_frame( void ) const { return show_; }

  bool operator==( const Frame & other ) const;

  void analyze_dependencies( const ReferenceDependency & deps ) const;
};

using KeyFrame = Frame<KeyFrameHeader, KeyFrameMacroblock>;
using InterFrame = Frame<InterFrameHeader, InterFrameMacroblock>;
using RefUpdateFrame = Frame<RefUpdateFrameHeader, RefUpdateFrameMacroblock>;
using StateUpdateFrame = Frame<StateUpdateFrameHeader, StateUpdateFrameMacroblock>;

/* Tracks which macroblocks we depend on from a raster */
class ReferenceDependency
{
private:
  unsigned width_, height_;
  SafeArray<std::vector<std::vector<bool>>, 3> needed_macroblocks_;
public:
  ReferenceDependency( const TwoD<InterFrameMacroblock> & frame_macroblocks );

  bool need_update_macroblock( const reference_frame & ref, const unsigned col, const unsigned row ) const;

  unsigned num_macroblocks_horiz() const { return width_; }
  unsigned num_macroblocks_vert() const { return height_; }
};

/* Encapsulates whatever needs to be done to update a reference for continuations */
class ReferenceUpdater
{
public:
  using Residue = SafeArray<SafeArray<int16_t, 4>, 4>;
  class MacroblockDiff {
  private:
    SafeArray<SafeArray<Residue, 4>, 4> Y_;
    SafeArray<SafeArray<Residue, 2>, 2> U_;
    SafeArray<SafeArray<Residue, 2>, 2> V_;
  public:
    MacroblockDiff();
    MacroblockDiff( const VP8Raster::Macroblock & lhs, const VP8Raster::Macroblock & rhs );

    Residue y_residue( const unsigned int column, const unsigned int row ) const;
    Residue u_residue( const unsigned int column, const unsigned int row ) const;
    Residue v_residue( const unsigned int column, const unsigned int row ) const;
  };
private:
  RasterHandle new_reference_;
  std::vector<std::vector<Optional<ReferenceUpdater::MacroblockDiff>>> diffs_;
  uint8_t prob_skip_;
  unsigned width_, height_;
public:
  ReferenceUpdater( const reference_frame & ref_frame, const VP8Raster & current,
                    const VP8Raster & target, const ReferenceDependency & dependencies );

  Optional<ReferenceUpdater::MacroblockDiff> macroblock( const unsigned int column, const unsigned int row ) const;

  uint8_t skip_probability() const;

  unsigned width() const { return width_; }
  unsigned height() const { return height_; }

  const RasterHandle new_reference() const { return new_reference_; }
};

#endif /* FRAME_HH */
