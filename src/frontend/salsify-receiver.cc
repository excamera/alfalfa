/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>
#include <random>

#include "socket.hh"
#include "packet.hh"
#include "optional.hh"
#include "player.hh"
#include "display.hh"

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

void display_frame( FramePlayer & player, VideoDisplay & display, const Chunk & frame )
{
  const Optional<RasterHandle> raster = player.decode( frame );

  if ( raster.initialized() ) {
    display.draw( raster.get() );
  }
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

  /* construct FramePlayer */
  FramePlayer player( paranoid_atoi( argv[ 2 ] ), paranoid_atoi( argv[ 3 ] ) );

  /* construct VideoDisplay */
  VideoDisplay display { player.example_raster() };

  size_t next_frame_no = 0;

  Optional<FragmentedFrame> current_frame;
  while ( true ) {
    /* wait for next UDP datagram */
    const auto new_fragment = socket.recv();

    /* parse into Packet */
    const Packet packet { new_fragment.payload };

    if ( packet.frame_no() < next_frame_no ) {
      /* we're not interested in this packet anymore */
      continue;
    }
    else if ( packet.frame_no() > next_frame_no ) {
      /* current frame is not finished yet, but we just received a packet
         for the next frame, so here we just encode the partial frame and
         display it and move on to the next frame */
      cerr << "got a packet for frame #" << packet.frame_no()
           << ", display previous frame(s)." << endl;

      display_frame( player, display, current_frame.get().partial_frame() );

      next_frame_no = packet.frame_no();
      current_frame.clear();
      current_frame.initialize( connection_id, packet );
      continue;
    }

    /* add to current frame */
    if ( current_frame.initialized() ) {
      current_frame.get().add_packet( packet );
    } else {
      current_frame.initialize( connection_id, packet );
    }

    /* is the frame ready to be decoded? */
    if ( current_frame.get().complete() ) {
      cerr << "decoding frame " << current_frame.get().frame_no() << endl;

      display_frame( player, display, current_frame.get().frame() );

      next_frame_no++;
      current_frame.clear();
    }
  }

  return EXIT_SUCCESS;
}
