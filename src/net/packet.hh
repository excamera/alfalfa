/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef PACKET_HH
#define PACKET_HH

#include <vector>
#include <cassert>

#include "chunk.hh"
#include "socket.hh"
#include "exception.hh"

class Packet
{
private:
  uint16_t connection_id_;
  uint32_t frame_no_;
  uint16_t fragment_no_;
  uint16_t fragments_in_this_frame_;

  std::string payload_;

  static std::string put_header_field( const uint16_t n )
  {
    const uint16_t network_order = htole16( n );
    return std::string( reinterpret_cast<const char *>( &network_order ),
                        sizeof( network_order ) );
  }

  static std::string put_header_field( const uint32_t n )
  {
    const uint32_t network_order = htole32( n );
    return std::string( reinterpret_cast<const char *>( &network_order ),
                        sizeof( network_order ) );
  }

public:
  static constexpr size_t MAXIMUM_PAYLOAD = 1400;

  /* getters */
  uint16_t connection_id() const { return connection_id_; }
  uint32_t frame_no() const { return frame_no_; }
  uint16_t fragment_no() const { return fragment_no_; }
  uint16_t fragments_in_this_frame() const { return fragments_in_this_frame_; }
  const std::string & payload() const { return payload_; }

  /* construct outgoing Packet */
  Packet( const std::vector<uint8_t> & whole_frame,
          const uint16_t connection_id,
          const uint32_t frame_no,
          const uint16_t fragment_no,
          size_t & next_fragment_start )
    : connection_id_( connection_id ),
      frame_no_( frame_no ),
      fragment_no_( fragment_no ),
      fragments_in_this_frame_( 0 ), /* temp value */
      payload_()
  {
    assert( not whole_frame.empty() );

    size_t first_byte = MAXIMUM_PAYLOAD * fragment_no;
    assert( first_byte < whole_frame.size() );

    size_t length = std::min( whole_frame.size() - first_byte,
                              MAXIMUM_PAYLOAD );
    assert( first_byte + length <= whole_frame.size() );

    payload_ = std::string( reinterpret_cast<const char*>( &whole_frame.at( first_byte ) ), length );

    next_fragment_start = first_byte + length;
  }

  /* construct incoming Packet */
  Packet( const Chunk & str )
    : connection_id_( str( 0, 2 ).le16() ),
      frame_no_( str( 2, 4 ).le32() ),
      fragment_no_( str( 6, 2 ).le16() ),
      fragments_in_this_frame_( str( 8, 2 ).le16() ),
      payload_( str( 10 ).to_string() )
  {
    if ( fragment_no_ >= fragments_in_this_frame_ ) {
      throw std::runtime_error( "invalid packet: fragment_no_ >= fragments_in_this_frame" );
    }

    if ( payload_.empty() ) {
      throw std::runtime_error( "invalid packet: empty payload" );
    }
  }

  /* serialize a Packet */
  std::string to_string() const
  {
    assert( fragments_in_this_frame_ > 0 );

    return put_header_field( connection_id_ )
      + put_header_field( frame_no_ )
      + put_header_field( fragment_no_ )
      + put_header_field( fragments_in_this_frame_ )
      + payload_;
  }

  void set_fragments_in_this_frame( const uint16_t x )
  {
    fragments_in_this_frame_ = x;
    assert( fragment_no_ < fragments_in_this_frame_ );
  }
};

class FragmentedFrame
{
private:
  uint16_t connection_id_;
  uint32_t frame_no_;
  uint16_t fragments_in_this_frame_;

  std::vector<Packet> fragments_;

public:
  /* construct outgoing FragmentedFrame */
  FragmentedFrame( const uint16_t connection_id,
                   const uint32_t frame_no,
                   const std::vector<uint8_t> & whole_frame )
    : connection_id_( connection_id ),
      frame_no_( frame_no ),
      fragments_in_this_frame_(),
      fragments_()
  {
    size_t next_fragment_start = 0;
    for ( uint16_t fragment_no = 0;
          next_fragment_start < whole_frame.size();
          fragment_no++ ) {
      fragments_.emplace_back( whole_frame, connection_id, frame_no,
                               fragment_no, next_fragment_start );
    }

    fragments_in_this_frame_ = fragments_.size();

    for ( Packet & packet : fragments_ ) {
      packet.set_fragments_in_this_frame( fragments_in_this_frame_ );
    }
  }

  /* construct incoming FragmentedFrame from a Packet */
  FragmentedFrame( const uint16_t connection_id,
                   const Packet & packet )
    : connection_id_( connection_id ),
      frame_no_( packet.frame_no() ),
      fragments_in_this_frame_( packet.fragments_in_this_frame() ),
      fragments_()
  {
    if ( packet.fragment_no() != 0 ) {
      throw std::runtime_error( "XXX unimplemented: starting a FragmentedFrame with Packet != #0" );
    }

    sanity_check( packet );

    fragments_.push_back( packet );
  }

  void sanity_check( const Packet & packet ) const {
    if ( packet.connection_id() != connection_id_ ) {
      std::cerr << packet.connection_id() << " vs. " << connection_id_ << "\n";
      throw std::runtime_error( "invalid packet, connection_id mismatch" );
    }

    if ( packet.fragments_in_this_frame() != fragments_in_this_frame_ ) {
      throw std::runtime_error( "invalid packet, fragments_in_this_frame mismatch" );
    }

    if ( packet.frame_no() != frame_no_ ) {
      throw std::runtime_error( "invalid packet, frame_no mismatch" );
    }

    if ( packet.fragment_no() >= fragments_in_this_frame_ ) {
      throw std::runtime_error( "invalid packet, fragment_no >= fragments_in_this_frame" );
    }
  }

  /* read a new packet */
  void add_packet( const Packet & packet )
  {
    sanity_check( packet );

    if ( packet.fragment_no() != fragments_.size() ) {
      throw std::runtime_error( "XXX unimplemented: out-of-order packets" );
    }

    fragments_.push_back( packet );
  }

  /* send */
  void send( UDPSocket & socket )
  {
    if ( fragments_.size() != fragments_in_this_frame_ ) {
      throw std::runtime_error( "attempt to send unfinished FragmentedFrame" );
    }

    for ( const Packet & packet : fragments_ ) {
      socket.send( packet.to_string() );
    }
  }

  bool complete() const
  {
    return fragments_.size() == fragments_in_this_frame_;
  }

  /* getters */
  uint16_t connection_id() const { return connection_id_; }
  uint32_t frame_no() const { return frame_no_; }
  uint16_t fragments_in_this_frame() const { return fragments_in_this_frame_; }
  std::string frame() const
  {
    std::string ret;
    for ( const auto & fragment : fragments_ ) {
      ret.append( fragment.payload() );
    }
    return ret;
  }
};

#endif /* PACKET_HH */
