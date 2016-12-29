/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <thread>

#include "yuv4mpeg.hh"
#include "encoder.hh"
#include "socket.hh"
#include "packet.hh"

using namespace std;

struct StateInfo
{
  uint16_t connection_id;
  uint32_t last_acked_frame;
  uint64_t last_ack_timestamp;
} state_info;

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

void recv_ack_packet( UDPSocket & socket )
{
  while ( true ) {
    auto packet = socket.recv();
    AckPacket ack( packet.payload );

    if ( ack.connection_id() != state_info.connection_id ) continue;

    state_info.last_acked_frame = ack.frame_no();
    state_info.last_ack_timestamp = packet.timestamp;

    cerr << "ACK(frame_no: " << ack.frame_no() << ")" << endl;
  }
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
  state_info.connection_id = connection_id;

  /* construct Socket for outgoing datagrams */
  UDPSocket socket;
  socket.connect( Address( argv[ 3 ], argv[ 4 ] ) );

  /* create a thread to receive ack packets */
  thread recv_thread( recv_ack_packet, ref( socket ) );
  recv_thread.detach();

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
