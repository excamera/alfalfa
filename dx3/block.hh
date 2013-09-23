#ifndef BLOCK_HH
#define BLOCK_HH

#include <string>
#include <stdexcept>

class Block
{
private:
  char *buffer_;
  uint64_t size_;

  void bounds_check( const uint64_t & offset, const uint64_t & length ) const
  {
    if ( offset + length > size_ ) {
      throw std::out_of_range( "attempted to read past end of block" );
    }
  }

public:
  Block( char * const s_buffer, const uint64_t & s_size )
    : buffer_( s_buffer ),
      size_( s_size )
  {}

  const char * buffer( void ) const { return buffer_; }
  char * mutable_buffer( void ) { return buffer_; }
  const uint64_t & size( void ) const { return size_; }

  Block operator() ( const uint64_t & offset, const uint64_t & length ) const
  {
    bounds_check( offset, length );
    return Block( buffer_ + offset, length );
  }

  std::string string( void ) const
  {
    return std::string( buffer_, size_ );
  }

  uint8_t octet( void ) const
  {
    bounds_check( 0, 1 );
    return *reinterpret_cast<const uint8_t *>( buffer_ );
  }

  uint16_t le16( void ) const
  {
    bounds_check( 0, 2 );
    return le16toh( *reinterpret_cast<const uint16_t *>( buffer_ ) );
  }

  uint64_t le32( void ) const
  {
    bounds_check( 0, 4 );
    return le32toh( *reinterpret_cast<const uint64_t *>( buffer_ ) );
  }

  uint64_t le64( void ) const
  {
    bounds_check( 0, 8 );
    return le64toh( *reinterpret_cast<const uint64_t *>( buffer_ ) );
  }
};

#endif /* BLOCK_HH */
