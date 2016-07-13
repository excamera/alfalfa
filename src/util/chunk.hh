/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef CHUNK_HH
#define CHUNK_HH

#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>

class Chunk
{
private:
  const uint8_t *buffer_;
  uint64_t size_;

  template <typename Type>
  Type extract_value() const
  {
    bounds_check( sizeof( Type ) );

    Type val;
    std::memcpy( &val, buffer_, sizeof( Type ) );
    return val;
  }

  void bounds_check( const uint64_t & length ) const
  {
    if ( length > size_ ) {
      throw std::out_of_range( "attempted to read past end of chunk" );
    }
  }

  static uint64_t bit_mask( const uint64_t & n )
  {
    if ( n > 63 ) {
      throw std::out_of_range( "bit mask size is unsupported" );
    }
    return ( 1 << n ) - 1;
  }

public:
  Chunk( const uint8_t *s_buffer, const uint64_t & s_size )
    : buffer_( s_buffer ),
      size_( s_size )
  {}

  Chunk( const std::string & str )
    : buffer_( reinterpret_cast<const uint8_t *>( str.data() ) ),
      size_( str.size() )
  {}

  Chunk( const std::vector< uint8_t > & vec )
    : buffer_( reinterpret_cast<const uint8_t *>( vec.data() ) ),
      size_( vec.size() )
  {}

  const uint8_t * buffer( void ) const { return buffer_; }
  const uint64_t & size( void ) const { return size_; }

  Chunk operator() ( const uint64_t & offset ) const
  {
    return operator() ( offset, size_ - offset );
  }

  Chunk operator() ( const uint64_t & offset, const uint64_t & length ) const
  {
    bounds_check( offset );
    bounds_check( offset + length );
    return Chunk( buffer_ + offset, length );
  }

  std::string to_string( void ) const
  {
    return std::string( reinterpret_cast<const char *>( buffer_ ), size_ );
  }

  const uint8_t & octet( void ) const
  {
    bounds_check( sizeof( uint8_t ) );
    return *buffer_;
  }

  uint16_t le16( void ) const
  {
    return le16toh( extract_value<uint16_t>() );
  }

  uint64_t le32( void ) const
  {
    return le32toh( extract_value<uint32_t>() );
  }

  uint64_t le64( void ) const
  {
    return le64toh( extract_value<uint64_t>() );
  }

  uint64_t bits( const uint64_t & bit_offset, const uint64_t bit_length ) const
  {
    const uint64_t byte_len = 1 + ( bit_offset + bit_length - 1 ) / 8;
    bounds_check( byte_len );
    if ( byte_len > sizeof( uint64_t ) ) {
      throw std::out_of_range( "bit offset and length not supported" );
    }

    uint64_t val = 0;
    for ( uint64_t i = 0; i < byte_len; i++ ) {
      val |= buffer_[ i ] << ( i * 8 );
    }

    return ( val >> bit_offset ) & bit_mask( bit_length );
  }
};

#endif /* CHUNK_HH */
