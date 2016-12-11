/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <vector>
#include <random>

#include "yuv4mpeg.hh"
#include "encoder.hh"
#include "socket.hh"

using namespace std;

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " INPUT QUANTIZER HOST PORT" << endl;
}

unsigned int paranoid_atoi( const string & in )
{
  const unsigned int ret = stoul( in );
  const string roundtrip = to_string( ret );
  if ( roundtrip != in ) {
    throw runtime_error( "invalid unsigned integer: " + in );
  }
  return ret;
}

class Packet
{
private:
  uint16_t connection_id_;
  uint32_t frame_no_;
  uint16_t fragment_no_;
  uint16_t fragments_in_this_frame_;

  string payload_;

  static string put_header_field( const uint16_t n )
  {
    const uint16_t network_order = htobe16( n );
    return string( reinterpret_cast<const char *>( &network_order ),
                   sizeof( network_order ) );
  }

  static string put_header_field( const uint32_t n )
  {
    const uint32_t network_order = htobe32( n );
    return string( reinterpret_cast<const char *>( &network_order ),
                   sizeof( network_order ) );
  }

public:
  static constexpr size_t MAXIMUM_PAYLOAD = 1400;

  /* getters */
  uint16_t connection_id() const { return connection_id_; }
  uint32_t frame_no() const { return frame_no_; }
  uint16_t fragment_no() const { return fragment_no_; }
  uint16_t fragments_in_this_frame() const { return fragments_in_this_frame_; }
  const string & payload() const { return payload_; }

  /* construct outgoing Packet */
  Packet( const vector<uint8_t> & whole_frame,
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

    size_t length = min( whole_frame.size() - first_byte,
                         MAXIMUM_PAYLOAD );
    assert( first_byte + length <= whole_frame.size() );

    payload_ = string( reinterpret_cast<const char*>( &whole_frame.at( first_byte ) ), length );

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
      throw runtime_error( "invalid packet: fragment_no_ >= fragments_in_this_frame" );
    }

    if ( payload_.empty() ) {
      throw runtime_error( "invalid packet: empty payload" );
    }
  }

  /* serialize a Packet */
  string to_string() const
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
  
  vector<Packet> fragments_;

public:
  /* construct outgoing FragmentedFrame */
  FragmentedFrame( const uint16_t connection_id,
                   const uint32_t frame_no,
                   const vector<uint8_t> & whole_frame )
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
  FragmentedFrame( const Packet & packet )
    : connection_id_( packet.connection_id() ),
      frame_no_( packet.frame_no() ),
      fragments_in_this_frame_( packet.fragments_in_this_frame() ),
      fragments_()
  {
    if ( packet.fragment_no() != 0 ) {
      throw runtime_error( "XXX unimplemented: starting a FragmentedFrame with Packet != #0" );
    }

    sanity_check( packet );

    fragments_.push_back( packet );
  }

  void sanity_check( const Packet & packet ) const {
    if ( packet.connection_id() != connection_id_ ) {
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
  void add_packet( const Packet & packet )
  {
    sanity_check( packet );

    if ( packet.fragment_no() != fragments_.size() ) {
      throw runtime_error( "XXX unimplemented: out-of-order packets" );
    }

    fragments_.push_back( packet );
  }

  /* send */
  void send( UDPSocket & socket )
  {
    if ( fragments_.size() != fragments_in_this_frame_ ) {
      throw runtime_error( "attempt to send unfinished FragmentedFrame" );
    }

    for ( const Packet & packet : fragments_ ) {
      socket.send( packet.to_string() );
    }
  }
};

uint16_t ezrand()
{
  random_device rd;
  uniform_int_distribution<uint16_t> ud;

  return ud( rd );
}

int main( int argc, char *argv[] )
{
  /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  if ( argc != 5 ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  /* choose a random connection_id */
  const uint16_t connection_id = ezrand();
  cerr << "Connection ID: " << connection_id << endl;

  /* open the YUV4MPEG input */
  YUV4MPEGReader input { argv[ 1 ] };

  /* get quantizer argument */
  const unsigned int y_ac_qi = paranoid_atoi( argv[ 2 ] );

  /* construct Socket for outgoing datagrams */
  UDPSocket socket;
  socket.connect( Address( argv[ 3 ], argv[ 4 ] ) );

  /* construct the encoder */
  Encoder encoder { input.display_width(), input.display_height(), false /* two-pass */, REALTIME_QUALITY };

  /* encode frames to stdout */
  unsigned int frame_no = 0;
  for ( Optional<RasterHandle> raster = input.get_next_frame();
        raster.initialized();
        raster = input.get_next_frame(), frame_no++ ) {
    cerr << "Encoding frame #" << frame_no << "...";

    const auto encode_beginning = chrono::system_clock::now();
    vector<uint8_t> frame = encoder.encode_with_quantizer( raster.get(), y_ac_qi );
    const auto encode_ending = chrono::system_clock::now();
    const int ms_elapsed = chrono::duration_cast<chrono::milliseconds>( encode_ending - encode_beginning ).count();
    cerr << "done (" << ms_elapsed << " ms, size=" << frame.size() << ")." << endl;

    cerr << "Sending frame #" << frame_no << "...";
    FragmentedFrame ff { connection_id, frame_no, frame };
    ff.send( socket );
    cerr << "done." << endl;
  }

  return EXIT_SUCCESS;
}
