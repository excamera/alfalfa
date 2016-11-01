/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef DECODER_HH
#define DECODER_HH

#include <vector>
#include "safe_array.hh"
#include "modemv_data.hh"
#include "loopfilter.hh"
#include "vp8_prob_data.hh"
#include "quantization.hh"
#include "exception.hh"
#include "raster_handle.hh"
#include "uncompressed_chunk.hh"
#include "frame_header.hh"
#include "enc_state_serializer.hh"

class Chunk;
class VP8Raster;
struct KeyFrameHeader;
struct InterFrameHeader;

template<unsigned int size>
static void assign( SafeArray< Probability, size > & dest, const Array< Unsigned<8>, size > & src )
{
  for ( unsigned int i = 0; i < size; i++ ) {
    dest.at( i ) = src.at( i );
  }
}

struct ProbabilityTables
{
  SafeArray<SafeArray<SafeArray<SafeArray<Probability,
                                          ENTROPY_NODES>,
                                PREV_COEF_CONTEXTS>,
                      COEF_BANDS>,
            BLOCK_TYPES> coeff_probs = k_default_coeff_probs;

  ProbabilityArray<num_y_modes> y_mode_probs = k_default_y_mode_probs;
  ProbabilityArray<num_uv_modes> uv_mode_probs = k_default_uv_mode_probs;

  SafeArray<SafeArray<Probability, MV_PROB_CNT>, 2> motion_vector_probs = k_default_mv_probs;

  ProbabilityTables() {}

  ProbabilityTables(EncoderStateDeserializer &idata);

  template <class HeaderType>
  void coeff_prob_update( const HeaderType & header );
  void mv_prob_update( const Enumerate<Enumerate<MVProbUpdate, MV_PROB_CNT>, 2> & mv_prob_updates );

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const ProbabilityTables & other ) const;

  bool operator!=( const ProbabilityTables & other ) const { return not operator==( other ); }

  uint32_t serialize(EncoderStateSerializer &odata) const;
  static ProbabilityTables deserialize(EncoderStateDeserializer &idata) {
    return ProbabilityTables(idata);
  }
};

struct FilterAdjustments
{
  /* Adjustments to the deblocking filter based on the macroblock's reference frame */
  SafeArray< int8_t, num_reference_frames > loopfilter_ref_adjustments {{}};

  /* Adjustments based on the macroblock's prediction mode */
  SafeArray< int8_t, 4 > loopfilter_mode_adjustments {{}};

  FilterAdjustments() {}

  FilterAdjustments(EncoderStateDeserializer &idata);

  template <class HeaderType>
  FilterAdjustments( const HeaderType & header ) { update( header ); }

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const FilterAdjustments & other ) const;

  bool operator!=( const FilterAdjustments & other ) const { return not operator==( other ); }

  size_t serialize(EncoderStateSerializer &odata) const;
  static FilterAdjustments deserialize(EncoderStateDeserializer &idata) {
    return FilterAdjustments(idata);
  }
};

struct References
{
  RasterHandle last, golden, alternative;

  References( const uint16_t width, const uint16_t height );

  References( MutableRasterHandle && raster );

  References(EncoderStateDeserializer &idata, const uint16_t width, const uint16_t height);

  const VP8Raster & at( const reference_frame reference_id ) const
  {
    switch ( reference_id ) {
    case LAST_FRAME: return last;
    case GOLDEN_FRAME: return golden;
    case ALTREF_FRAME: return alternative;
    default: throw LogicError();
    }
  }

  bool operator==( const References & other ) const;

  bool operator!=( const References & other ) const { return not operator==( other ); }

  size_t serialize(EncoderStateSerializer &odata) const;
  static References deserialize(EncoderStateDeserializer &idata);
};

class SafeReferences
{
public:
  static const size_t MARGIN_WIDTH = 128;

  class SafeReference
  {
  private:
    TwoD<uint8_t> safe_Y_;

  public:
    SafeReference( uint16_t width, uint16_t height )
      : safe_Y_( width + MARGIN_WIDTH * 2, height + MARGIN_WIDTH * 2 )
    {}

    SafeReference( RasterHandle source )
      : SafeReference( source.get().width(), source.get().height() )
    {
      copy_raster( source );
    }

    void copy_raster( RasterHandle source )
    {
      const TwoD<uint8_t> & original_Y = source.get().Y();
      const uint16_t original_width = original_Y.width();
      const uint16_t original_height = original_Y.height();

      /*
           1   |   2   |   3
        -----------------------
           4   |   5   |   6
        -----------------------
           7   |   8   |   9

        The original image goes into 5.
      */


      for ( size_t nrow = 0; nrow < safe_Y_.height(); nrow++ ) {
        if ( nrow < MARGIN_WIDTH ) {
          memset( &safe_Y_.at( 0, nrow ),
                  original_Y.at( 0, 0 ),
                  MARGIN_WIDTH ); // (1)

          memcpy( &safe_Y_.at( MARGIN_WIDTH, nrow ),
                  &original_Y.at( 0, 0 ),
                  original_width ); // (2)

          memset( &safe_Y_.at( MARGIN_WIDTH + original_width, nrow ),
                  original_Y.at( original_width - 1, 0 ),
                  MARGIN_WIDTH ); // (3)
        }
        else if ( nrow > MARGIN_WIDTH + original_Y.height() ) {
          memset( &safe_Y_.at( 0, nrow ),
                  original_Y.at( 0, original_height - 1 ),
                  MARGIN_WIDTH ); // (7)

          memcpy( &safe_Y_.at( MARGIN_WIDTH, nrow ),
                  &original_Y.at( 0, original_height - 1 ),
                  original_width ); // (8)

          memset( &safe_Y_.at( MARGIN_WIDTH + original_width, nrow ),
                  original_Y.at( original_width - 1, 0 ),
                  MARGIN_WIDTH ); // (9)
        }
        else {
          memset( &safe_Y_.at( 0, nrow ),
                  original_Y.at( 0, nrow - MARGIN_WIDTH ),
                  MARGIN_WIDTH ); // (4)

          memcpy( &safe_Y_.at( MARGIN_WIDTH, nrow ),
                  &original_Y.at( 0, nrow - MARGIN_WIDTH ),
                  original_width ); // (5)

          memset( &safe_Y_.at( MARGIN_WIDTH + original_width, nrow ),
                  original_Y.at( original_width - 1, nrow - MARGIN_WIDTH ),
                  MARGIN_WIDTH ); // (6)
        }
      }


    }
  };

private:
  /* For now, we only need the Y planes to do the diamond search, so we only
     keep them in our safe references. */
  SafeReference last_, golden_, alternative_;

  /* Copies the Y plane of the given raster to the target buffer and automatically
     does the edge extension. */

public:
  SafeReferences( const uint16_t width, const uint16_t height )
    : last_( width, height ), golden_( width, height ),
      alternative_( width, height )
  {}

  SafeReferences( const References & references )
    : SafeReferences( references.last.get().width(), references.last.get().height() )
  {
    update_all_refs( references );
  }

  void update_ref( reference_frame reference_id, RasterHandle reference_raster )
  {
    switch ( reference_id ) {
    case LAST_FRAME: last_.copy_raster( reference_raster ); break;
    case GOLDEN_FRAME: golden_.copy_raster( reference_raster ); break;
    case ALTREF_FRAME: alternative_.copy_raster( reference_raster ); break;
    default: throw LogicError();
    }
  }

  void update_all_refs( const References & references )
  {
    update_ref( LAST_FRAME, references.last );
    update_ref( GOLDEN_FRAME, references.golden );
    update_ref( ALTREF_FRAME, references.alternative );
  }

  // XXX remove these
  const SafeReference & last() const { return last_; }
  const SafeReference & golden() const { return golden_; }
  const SafeReference & alternative() const { return alternative_; }

  const SafeReference & get( reference_frame reference_id ) const
  {
    switch ( reference_id ) {
    case LAST_FRAME: return last_;
    case GOLDEN_FRAME: return golden_;
    case ALTREF_FRAME: return alternative_;
    default: throw LogicError();
    }
  }
};

using SegmentationMap = TwoD< uint8_t >;

struct Segmentation
{
  /* Whether segment-based adjustments are absolute or relative */
  bool absolute_segment_adjustments {};

  /* Segment-based adjustments to the quantizer */
  SafeArray< int8_t, num_segments > segment_quantizer_adjustments {{}};

  /* Segment-based adjustments to the in-loop deblocking filter */
  SafeArray< int8_t, num_segments > segment_filter_adjustments {{}};

  /* Mapping of macroblocks to segments */
  SegmentationMap map;

  template <class HeaderType>
  Segmentation( const HeaderType & header,
                const unsigned int width,
                const unsigned int height );

  Segmentation(EncoderStateDeserializer &idata, const bool abs, const unsigned width, const unsigned height);

  // testing only
  Segmentation(const unsigned width, const unsigned height);

  template <class HeaderType>
  void update( const HeaderType & header );

  size_t hash( void ) const;

  bool operator==( const Segmentation & other ) const;

  Segmentation( const Segmentation & other );

  size_t serialize(EncoderStateSerializer &odata) const;
  static Segmentation deserialize(EncoderStateDeserializer &idata);
};

struct DecoderState
{
  uint16_t width, height;

  ProbabilityTables probability_tables = {};
  Optional<Segmentation> segmentation = {};
  Optional<FilterAdjustments> filter_adjustments = {};

  // testing only
  DecoderState(const unsigned s_width, const unsigned s_height,
               ProbabilityTables &&p, Optional<Segmentation> &&s,
               Optional<FilterAdjustments> &&f);

  DecoderState(EncoderStateDeserializer &idata, const unsigned s_width, const unsigned s_height);

  DecoderState( const unsigned int s_width, const unsigned int s_height );

  DecoderState( const KeyFrameHeader & header,
                const unsigned int s_width,
                const unsigned int s_height );

  template <class FrameType>
  FrameType parse_and_apply( const UncompressedChunk & uncompressed_chunk );

  bool operator==( const DecoderState & other ) const;

  bool operator!=( const DecoderState & other ) const { return not operator==( other ); }

  Optional<ModeRefLFDeltaUpdate> get_filter_update( void ) const;
  Optional<SegmentFeatureData> get_segment_update( void ) const;

  size_t hash( void ) const;

  size_t serialize(EncoderStateSerializer &odata) const;
  static DecoderState deserialize(EncoderStateDeserializer &idata);
};

class DecoderHash
{
private:
  size_t state_hash_, last_hash_, golden_hash_, alt_hash_;
public:
  DecoderHash( size_t state_hash, size_t last_hash,
               size_t golden_hash, size_t alt_hash );

  size_t hash( void ) const;

  std::string str( void ) const;

  bool operator==( const DecoderHash & other ) const;

  bool operator!=( const DecoderHash & other ) const;
};

class Decoder
{
private:
  DecoderState state_;
  References references_;

public:
  Decoder( const uint16_t width, const uint16_t height );
  Decoder( DecoderState state, References references );
  Decoder( EncoderStateDeserializer &idata );

  const VP8Raster & example_raster( void ) const { return references_.last; }

  DecoderHash get_hash( void ) const;

  UncompressedChunk decompress_frame( const Chunk & compressed_frame ) const;

  template<class FrameType>
  FrameType parse_frame( const UncompressedChunk & decompressed_frame );

  template<class FrameType>
  std::pair<bool, RasterHandle> decode_frame( const FrameType & frame );

  std::pair<bool, RasterHandle> get_frame_output( const Chunk & compressed_frame );
  Optional<RasterHandle> parse_and_decode_frame( const Chunk & compressed_frame );

  template <class FrameType>
  void apply_decoded_frame( const FrameType & frame, const RasterHandle & output, const Decoder & target )
  {
    state_ = target.state_;
    frame.copy_to( output, references_ );
  }

  DecoderState get_state() const { return state_; }

  References get_references( void ) const { return references_; }

  uint16_t get_width() const { return state_.width; }
  uint16_t get_height() const { return state_.height; }

  bool operator==( const Decoder & other ) const;

  bool operator!=( const Decoder & other ) const { return not operator==( other ); }

  uint32_t minihash() const;

  bool minihash_match( const uint32_t other_minihash ) const;

  size_t serialize(EncoderStateSerializer &odata) const;

  static Decoder deserialize(EncoderStateDeserializer &idata);
};


#endif /* DECODER_HH */
