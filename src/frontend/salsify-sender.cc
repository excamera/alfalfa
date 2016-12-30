/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <limits>

#include "yuv4mpeg.hh"
#include "encoder.hh"
#include "socket.hh"
#include "packet.hh"
#include "poller.hh"

using namespace std;
using namespace std::chrono;
using namespace PollerShortNames;

struct StateInfo
{
  uint16_t connection_id;
  uint32_t last_acked_frame;
  vector<uint64_t> cumulative_frame_sizes;

  StateInfo( uint16_t connection_id )
    : connection_id( connection_id ),
      last_acked_frame( numeric_limits<uint32_t>::max() ),
      cumulative_frame_sizes()
  {}
};

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " INPUT QUANTIZER HOST PORT CONNECTION_ID" << endl;
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

int main( int argc, char *argv[] )
{
  /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  if ( argc != 6 ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  /* open the YUV4MPEG input */
  YUV4MPEGReader input { argv[ 1 ] };

  /* get quantizer argument */
  const unsigned int y_ac_qi = paranoid_atoi( argv[ 2 ] );

  /* get connection_id */
  const uint16_t connection_id = paranoid_atoi( argv[ 5 ] );

  /* construct Socket for outgoing datagrams */
  UDPSocket socket;
  socket.connect( Address( argv[ 3 ], argv[ 4 ] ) );
  socket.set_timestamps();

  /* keep some information about the current play state and average throughput */
  StateInfo state_info( connection_id );

  /* used to read ack packets */
  Poller poller;
  poller.add_action( Poller::Action( socket, Direction::In,
    [&]()
    {
      auto packet = socket.recv();
      AckPacket ack( packet.payload );

      if ( ack.connection_id() != state_info.connection_id ) {
        /* this is not an ack for this session! */
        return ResultType::Continue;
      }

      cerr << "ACK(frame: " << ack.frame_no()
           << ", fragment:" << ack.fragment_no() << ")" << endl;

      return ResultType::Continue;
    },
    [&]() { return not socket.eof(); }
  ) );

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

    state_info.cumulative_frame_sizes.push_back(
      ( frame_no ) ? state_info.cumulative_frame_sizes[ frame_no - 1 ] + frame.size()
                   : frame.size() );

    cerr << "Sending frame #" << frame_no << "...";
    FragmentedFrame ff { connection_id, frame_no, 0 /* time to next frame */, frame };
    ff.send( socket );
    cerr << "done." << endl;

    /* let's see if we have received any acks, but we don't wait for it. */
    poller.poll( 0 );
  }

  return EXIT_SUCCESS;
}
