/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>
#include <random>
#include <map>
#include <utility>
#include <tuple>

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
  if ( frame.size() == 0 ) {
    return;
  }

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
  player.set_error_concealment( true );

  /* construct VideoDisplay */
  VideoDisplay display { player.example_raster() };

  size_t next_frame_no = 0;

  /* frame no => FragmentedFrame; used when receiving packets out of order */
  map<size_t, FragmentedFrame> fragmented_frames;

  while ( true ) {
    /* wait for next UDP datagram */
    const auto new_fragment = socket.recv();

    /* parse into Packet */
    const Packet packet { new_fragment.payload };

    if ( packet.frame_no() < next_frame_no ) {
      /* we're not interested in this anymore */
      continue;
    }

    /* add to current frame */
    if ( fragmented_frames.count( packet.frame_no() ) ) {
      fragmented_frames.at( packet.frame_no() ).add_packet( packet );
    } else {
      /*
        This was judged "too fancy" by the Code Review Board of Dec. 29, 2016.

        fragmented_frames.emplace( std::piecewise_construct,
                                   forward_as_tuple( packet.frame_no() ),
                                   forward_as_tuple( connection_id, packet ) );
      */

      fragmented_frames.insert( make_pair( packet.frame_no(),
                                           FragmentedFrame( connection_id, packet ) ) );
    }

    /* is the next frame ready to be decoded? */
    if ( fragmented_frames.count( next_frame_no ) > 0 and fragmented_frames.at( next_frame_no ).complete() ) {
      cerr << "decoding frame " << next_frame_no << endl;

      display_frame( player, display, fragmented_frames.at( next_frame_no ).frame() );

      fragmented_frames.erase( next_frame_no );
      next_frame_no++;
    }
  }

  return EXIT_SUCCESS;
}
