/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

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

string Packet::put_header_field( const uint64_t n )
{
  const uint32_t network_order = htole64( n );
  return string( reinterpret_cast<const char *>( &network_order ),
                 sizeof( network_order ) );
}

Packet::Packet( const vector<uint8_t> & whole_frame,
                const uint16_t connection_id,
                const uint32_t source_state,
                const uint32_t target_state,
                const uint32_t frame_no,
                const uint16_t fragment_no,
                const uint16_t time_since_last,
                size_t & next_fragment_start )
  : valid_( true ),
    connection_id_( connection_id ),
    source_state_( source_state ),
    target_state_( target_state ),
    frame_no_( frame_no ),
    fragment_no_( fragment_no ),
    fragments_in_this_frame_( 0 ), /* temp value */
    time_since_last_( time_since_last ),
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
    source_state_( str( 2, 4 ).le32() ),
    target_state_( str( 6, 4 ).le32() ),
    frame_no_( str( 10, 4 ).le32() ),
    fragment_no_( str( 14, 2 ).le16() ),
    fragments_in_this_frame_( str( 16, 2 ).le16() ),
    time_since_last_( str( 18, 4 ).le32() ),
    payload_( str( 22 ).to_string() )
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
    source_state_(),
    target_state_(),
    frame_no_(),
    fragment_no_(),
    fragments_in_this_frame_(),
    time_since_last_(),
    payload_()
{}

/* serialize a Packet */
string Packet::to_string() const
{
  assert( fragments_in_this_frame_ > 0 );

  return put_header_field( connection_id_ )
       + put_header_field( source_state_ )
       + put_header_field( target_state_ )
       + put_header_field( frame_no_ )
       + put_header_field( fragment_no_ )
       + put_header_field( fragments_in_this_frame_ )
       + put_header_field( time_since_last_ )
       + payload_;
}

void Packet::set_fragments_in_this_frame( const uint16_t x )
{
  fragments_in_this_frame_ = x;
  assert( fragment_no_ < fragments_in_this_frame_ );
}

/* construct outgoing FragmentedFrame */
FragmentedFrame::FragmentedFrame( const uint16_t connection_id,
                                  const uint32_t source_state,
                                  const uint32_t target_state,
                                  const uint32_t frame_no,
                                  const uint32_t time_since_last,
                                  const vector<uint8_t> & whole_frame )
  : connection_id_( connection_id ),
    source_state_( source_state ),
    target_state_( target_state ),
    frame_no_( frame_no ),
    fragments_in_this_frame_(),
    fragments_(),
    remaining_fragments_( 0 )
{
  size_t next_fragment_start = 0;

  for ( uint16_t fragment_no = 0; next_fragment_start < whole_frame.size();
        fragment_no++ ) {
    fragments_.emplace_back( whole_frame, connection_id, source_state_, target_state_,
                             frame_no, fragment_no, 0, next_fragment_start );
  }

  fragments_.front().set_time_to_next( time_since_last );

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
    source_state_( packet.source_state() ),
    target_state_( packet.target_state() ),
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

  if ( packet.source_state() != source_state_ ) {
    throw runtime_error( "invalid packet, source_state mismatch" );
  }

  if ( packet.target_state() != target_state_ ) {
    throw runtime_error( "invalid packet, source_state mismatch" );
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

  assert( complete() );

  for ( const Packet & packet : fragments_ ) {
    socket.send( packet.to_string() );
  }
}

bool FragmentedFrame::complete() const
{
  return remaining_fragments_ == 0;
}

const vector<Packet> & FragmentedFrame::packets() const
{
  if ( (not complete()) or (fragments_.size() != fragments_in_this_frame_) ) {
    throw runtime_error( "attempt to access unfinished FragmentedFrame" );
  }

  return fragments_;
}

string FragmentedFrame::frame() const
{
  string ret;

  if ( not complete() ) {
    throw runtime_error( "attempt to build frame from unfinished FragmentedFrame" );
  }

  for ( const auto & fragment : fragments_ ) {
    ret.append( fragment.payload() );
  }

  return ret;
}

string FragmentedFrame::partial_frame() const
{
  string ret;

  for ( const auto & fragment : fragments_ ) {
    if ( not fragment.valid() ) {
      break;
    }

    ret.append( fragment.payload() );
  }

  return ret;
}

/* AckPacket */

AckPacket::AckPacket( const uint16_t connection_id, const uint32_t frame_no,
                      const uint16_t fragment_no, const uint32_t avg_delay,
                      const uint32_t current_state, deque<uint32_t> complete_states )
  : connection_id_( connection_id ), frame_no_( frame_no ),
    fragment_no_( fragment_no ), avg_delay_( avg_delay ),
    current_state_( current_state ), complete_states_( complete_states )
{}

AckPacket::AckPacket( const Chunk & str )
  : connection_id_( str( 0, 2 ).le16() ),
    frame_no_( str( 2, 4 ).le32() ),
    fragment_no_( str( 6, 2 ).le16() ),
    avg_delay_( str( 8, 4 ).le32() ),
    current_state_( str( 12, 4 ).le32() ),
    complete_states_( str( 16, 4 ).le32() )
{
  for ( size_t i = 0; i < complete_states_.size(); i++ ) {
    complete_states_[ i ] = str( 20 + i * 4, 4 ).le32();
  }
}

std::string AckPacket::to_string()
{
  string packet = Packet::put_header_field( connection_id_ )
                + Packet::put_header_field( frame_no_ )
                + Packet::put_header_field( fragment_no_ )
                + Packet::put_header_field( avg_delay_ )
                + Packet::put_header_field( current_state_ );

  packet += Packet::put_header_field( static_cast<uint32_t>( complete_states_.size() ) );

  for ( const auto state : complete_states_ ) {
    packet += Packet::put_header_field( state );
  }

  return packet;
}

void AckPacket::sendto( UDPSocket & socket, const Address & addr )
{
  socket.sendto( addr, to_string() );
}
