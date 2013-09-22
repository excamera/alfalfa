#ifndef IVF_HH
#define IVF_HH

#include <string>
#include <cstdint>

#include "file.hh"

class IVF
{
private:
  File file_;
  Block header_;

  std::string dkif( void ) const { return header_.string( 0, 4 ); }
  uint16_t version( void ) const { return header_.le16( 4 ); }
  uint16_t header_len( void ) const { return header_.le16( 6); }

public:
  IVF( const std::string & filename );

  /* IVF file header accessors */
  std::string fourcc( void ) const { return header_.string( 8, 4 ); }
  uint16_t width( void ) const { return header_.le16( 12 ); }
  uint16_t height( void ) const { return header_.le16( 14 ); }
  uint32_t frame_rate( void ) const { return header_.le32( 16 ); }
  uint32_t time_scale( void ) const { return header_.le32( 20 ); }
  uint32_t frame_count( void ) const { return header_.le32( 24 ); }
};

#endif /* IVF_HH */
