#ifndef DIFF_GENERATOR_HH
#define DIFF_GENERATOR_HH

#include "decoder.hh"
#include "frame.hh"
#include "serialized_frame.hh"

/* Specialized version of Decoder which holds all necessary
 * state for generating diffs between streams */
class DiffGenerator : public Decoder
{
private:
  bool on_key_frame_;
  Optional<KeyFrame> key_frame_;
  Optional<InterFrame> inter_frame_;

  /* Need to know the previous set of references to calculate which references
   * are missing for a continuation frame */
  References prev_references_;

public:
  DiffGenerator( const uint16_t width, const uint16_t height );

  DiffGenerator( const DiffGenerator & other );

  bool decode_frame( const Chunk & frame, RasterHandle & raster );

  std::string cur_frame_stats( void ) const;

  std::string source_hash_str( const Decoder & source ) const;

  std::string target_hash_str( void ) const;

  void set_references( bool set_last, bool set_golden, bool set_alt,
                       const DiffGenerator & other );

  std::array<bool, 3> missing_references( const DiffGenerator & other ) const;

  SerializedFrame operator-( const DiffGenerator & source_decoder ) const;

};
#endif
