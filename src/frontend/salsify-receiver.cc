/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>
#include <random>

#include "socket.hh"
#include "packet.hh"
#include "optional.hh"

using namespace std;

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " PORT WIDTH HEIGHT" << endl;
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

  if ( argc != 4 ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }
  
  /* choose a random connection_id */
  const uint16_t connection_id = ezrand();
  cerr << "Connection ID: " << connection_id << endl;

  /* construct Socket for incoming  datagrams */
  UDPSocket socket;
  socket.bind( Address( "0", argv[ 1 ] ) );

  Optional<FragmentedFrame> current_frame;
  while ( true ) {
    /* wait for next UDP datagram */
    const auto new_fragment = socket.recv();

    /* parse into Packet */
    const Packet packet { new_fragment.payload };

    /* add to current frame */
    if ( current_frame.initialized() ) {
      current_frame.get().add_packet( packet );
    } else {
      current_frame.initialize( connection_id, packet );
    }

    /* is the frame ready to be decoded? */
    if ( current_frame.get().complete() ) {
      cerr << "decoding frame " << current_frame.get().frame_no() << endl;
      current_frame.clear();
    }
  }

  return EXIT_SUCCESS;
}
