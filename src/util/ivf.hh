/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef IVF_HH
#define IVF_HH

#include <string>
#include <cstdint>
#include <vector>

#include "file.hh"

class IVF
{
private:
  File file_;
  Chunk header_;

  std::string fourcc_;
  uint16_t width_, height_;
  uint32_t frame_rate_, time_scale_, frame_count_;
  uint32_t expected_decoder_minihash_;

  std::vector< std::pair<uint64_t, uint32_t> > frame_index_;

public:
  static constexpr int supported_header_len = 32;
  static constexpr int frame_header_len = 12;

  IVF( const std::string & filename );

  const std::string & fourcc( void ) const { return fourcc_; }
  uint16_t width( void ) const { return width_; }
  uint16_t height( void ) const { return height_; }
  uint32_t frame_rate( void ) const { return frame_rate_; }
  uint32_t time_scale( void ) const { return time_scale_; }
  uint32_t frame_count( void ) const { return frame_count_; }

  Chunk frame( const uint32_t & index ) const;

  size_t size() const { return file_.size(); }

  uint32_t expected_decoder_minihash() const { return expected_decoder_minihash_; }
};

#endif /* IVF_HH */
