/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <string>
#include <algorithm>
#include <vector>

#include "packet.hh"

using namespace std;

string Packet::put_header_field( const uint16_t n )
{
  const uint16_t network_order = htole16( n );
  return string( reinterpret_cast<const char *>( &network_order ),
                 sizeof( network_order ) );
}

string Packet::put_header_field( const uint32_t n )
{
  const uint32_t network_order = htole32( n );
  return string( reinterpret_cast<const char *>( &network_order ),
                 sizeof( network_order ) );
}

Packet::Packet( const vector<uint8_t> & whole_frame,
                const uint16_t connection_id,
                const uint32_t frame_no,
                const uint16_t fragment_no,
                size_t & next_fragment_start )
  : valid_( true ),
    connection_id_( connection_id ),
    frame_no_( frame_no ),
    fragment_no_( fragment_no ),
    fragments_in_this_frame_( 0 ), /* temp value */
    payload_()
{
  assert( not whole_frame.empty() );

  size_t first_byte = MAXIMUM_PAYLOAD * fragment_no;
  assert( first_byte < whole_frame.size() );

  size_t length = min( whole_frame.size() - first_byte, MAXIMUM_PAYLOAD );
  assert( first_byte + length <= whole_frame.size() );

  payload_ = string( reinterpret_cast<const char*>( &whole_frame.at( first_byte ) ), length );

  next_fragment_start = first_byte + length;
}

/* construct incoming Packet */
Packet::Packet( const Chunk & str )
  : valid_( true ),
    connection_id_( str( 0, 2 ).le16() ),
    frame_no_( str( 2, 4 ).le32() ),
    fragment_no_( str( 6, 2 ).le16() ),
    fragments_in_this_frame_( str( 8, 2 ).le16() ),
    payload_( str( 10 ).to_string() )
{
  if ( fragment_no_ >= fragments_in_this_frame_ ) {
    throw runtime_error( "invalid packet: fragment_no_ >= fragments_in_this_frame" );
  }

  if ( payload_.empty() ) {
    throw runtime_error( "invalid packet: empty payload" );
  }
}

/* construct an empty, invalid packet */
Packet::Packet()
  : valid_( false ),
    connection_id_(),
    frame_no_(),
    fragment_no_(),
    fragments_in_this_frame_(),
    payload_()
{}

/* serialize a Packet */
string Packet::to_string() const
{
  assert( fragments_in_this_frame_ > 0 );

  return put_header_field( connection_id_ )
    + put_header_field( frame_no_ )
    + put_header_field( fragment_no_ )
    + put_header_field( fragments_in_this_frame_ )
    + payload_;
}

void Packet::set_fragments_in_this_frame( const uint16_t x )
{
  fragments_in_this_frame_ = x;
  assert( fragment_no_ < fragments_in_this_frame_ );
}

/* construct outgoing FragmentedFrame */
FragmentedFrame::FragmentedFrame( const uint16_t connection_id,
                                  const uint32_t frame_no,
                                  const vector<uint8_t> & whole_frame )
  : connection_id_( connection_id ),
    frame_no_( frame_no ),
    fragments_in_this_frame_(),
    fragments_(),
    remaining_fragments_( 0 )
{
  size_t next_fragment_start = 0;

  for ( uint16_t fragment_no = 0; next_fragment_start < whole_frame.size();
        fragment_no++ ) {
    fragments_.emplace_back( whole_frame, connection_id, frame_no,
                             fragment_no, next_fragment_start );
  }

  fragments_in_this_frame_ = fragments_.size();
  remaining_fragments_ = 0;

  for ( Packet & packet : fragments_ ) {
    packet.set_fragments_in_this_frame( fragments_in_this_frame_ );
  }
}

  /* construct incoming FragmentedFrame from a Packet */
FragmentedFrame::FragmentedFrame( const uint16_t connection_id,
                                  const Packet & packet )
  : connection_id_( connection_id ),
    frame_no_( packet.frame_no() ),
    fragments_in_this_frame_( packet.fragments_in_this_frame() ),
    fragments_( packet.fragments_in_this_frame() ),
    remaining_fragments_( packet.fragments_in_this_frame() )
{
  sanity_check( packet );

  add_packet( packet );
}

void FragmentedFrame::sanity_check( const Packet & packet ) const {
  if ( packet.connection_id() != connection_id_ ) {
    cerr << packet.connection_id() << " vs. " << connection_id_ << "\n";
    throw runtime_error( "invalid packet, connection_id mismatch" );
  }

  if ( packet.fragments_in_this_frame() != fragments_in_this_frame_ ) {
    throw runtime_error( "invalid packet, fragments_in_this_frame mismatch" );
  }

  if ( packet.frame_no() != frame_no_ ) {
    throw runtime_error( "invalid packet, frame_no mismatch" );
  }

  if ( packet.fragment_no() >= fragments_in_this_frame_ ) {
    throw runtime_error( "invalid packet, fragment_no >= fragments_in_this_frame" );
  }
}

/* read a new packet */
void FragmentedFrame::add_packet( const Packet & packet )
{
  sanity_check( packet );

  if ( not fragments_[ packet.fragment_no() ].valid() ) {
    remaining_fragments_--;
    fragments_[ packet.fragment_no() ] = packet;
  }
}

/* send */
void FragmentedFrame::send( UDPSocket & socket )
{
  if ( fragments_.size() != fragments_in_this_frame_ ) {
    throw runtime_error( "attempt to send unfinished FragmentedFrame" );
  }

  for ( const Packet & packet : fragments_ ) {
    socket.send( packet.to_string() );
  }
}

bool FragmentedFrame::complete() const
{
  return remaining_fragments_ == 0;
}

string FragmentedFrame::frame() const
{
  string ret;
  for ( const auto & fragment : fragments_ ) {
    ret.append( fragment.payload() );
  }
  return ret;
}
