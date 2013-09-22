#ifndef BLOCK_HH
#define BLOCK_HH

#include <string>

class Block
{
private:
  const char *buffer_;
  uint32_t size_;

public:
  Block( const char * const s_buffer, const uint32_t & s_size )
    : buffer_( s_buffer ),
      size_( s_size )
  {}

  std::string string( const uint32_t & offset, const uint32_t & size ) const
  {
    return std::string( buffer_ + offset, size );
  }

  uint8_t octet( const uint32_t & offset ) const
  {
    return *reinterpret_cast<const uint8_t *>(buffer_ + offset);
  }

  uint16_t le16( const uint32_t & offset ) const
  {
    return le16toh( *reinterpret_cast<const uint16_t *>(buffer_ + offset) );
  }

  uint32_t le32( const uint32_t & offset ) const
  {
    return le32toh( *reinterpret_cast<const uint32_t *>(buffer_ + offset) );
  }

  uint64_t le64( const uint32_t & offset ) const
  {
    return le64toh( *reinterpret_cast<const uint64_t *>(buffer_ + offset) );
  }
};

#endif /* BLOCK_HH */
