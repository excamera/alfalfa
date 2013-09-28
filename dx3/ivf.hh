#ifndef IVF_HH
#define IVF_HH

#include <string>
#include <cstdint>
#include <vector>

#include "file.hh"

class IVF
{
private:
  const int supported_header_len = 32;
  const int frame_header_len = 12;

  File file_;
  Block header_;

  std::string fourcc_;
  uint16_t width_, height_;
  uint32_t frame_rate_, time_scale_, frame_count_;

  std::vector< std::pair<uint64_t, uint32_t> > frame_index_;

public:
  IVF( const std::string & filename );

  const std::string & fourcc( void ) const { return fourcc_; }
  const uint16_t & width( void ) const { return width_; }
  const uint16_t & height( void ) const { return height_; }
  const uint32_t & frame_rate( void ) const { return frame_rate_; }
  const uint32_t & time_scale( void ) const { return time_scale_; }
  const uint32_t & frame_count( void ) const { return frame_count_; }

  Block frame( const uint32_t & index ) const;
};

#endif /* IVF_HH */
