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

class EncodeTimeEWMA
{
private:
  static constexpr double ALPHA = 0.25;

  double value_ { -1 };

public:
  void add( const uint64_t new_value )
  {
    if ( value_ < 0 ) {
      value_ = new_value;
    }
    else {
      value_ = ALPHA * new_value + ( 1 - ALPHA ) * value_;
    }
  }

  uint32_t int_value() { return static_cast<uint32_t>( value_ ); }
};

size_t target_size( uint32_t avg_delay )
{
  return static_cast<size_t>( ( 1e6 * 1400.0 / avg_delay ) / 24 /* fps */ );
}

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

  /* keep the average encode time */
  EncodeTimeEWMA avg_encode_time;

  /* average inter-packet delay, reported by receiver */
  uint32_t avg_delay = numeric_limits<uint32_t>::max();

  /* used to read ack packets */
  Poller poller;
  poller.add_action( Poller::Action( socket, Direction::In,
    [&]()
    {
      auto packet = socket.recv();
      AckPacket ack( packet.payload );

      if ( ack.connection_id() != connection_id ) {
        /* this is not an ack for this session! */
        return ResultType::Continue;
      }

      avg_delay = ack.avg_delay();

      cerr << "ACK(frame: " << ack.frame_no() << ", "
           << "fragment: " << ack.fragment_no() << ", "
           << "avg delay: " << ack.avg_delay()
           << ")" << endl;

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
    vector<uint8_t> frame;

    if ( avg_delay == numeric_limits<uint32_t>::max() ) {
      frame = encoder.encode_with_quantizer( raster.get(), y_ac_qi );
    }
    else {
      frame = encoder.encode_with_target_size( raster.get(), target_size( avg_delay ) );
    }

    const auto encode_ending = chrono::system_clock::now();
    const int us_elapsed = chrono::duration_cast<chrono::microseconds>( encode_ending - encode_beginning ).count();

    avg_encode_time.add( us_elapsed );

    cerr << "done (" << us_elapsed << " us, "
         << "avg=" << avg_encode_time.int_value()<< " us, "
         << "size=" << frame.size() << " bytes)." << endl;

    cerr << "Sending frame #" << frame_no << "...";
    FragmentedFrame ff { connection_id, frame_no, avg_encode_time.int_value() /* time to next frame */, frame };
    ff.send( socket );
    cerr << "done." << endl;

    /* let's see if we have received any acks, but we don't wait for it. */
    while ( true ) {
      if ( poller.poll( 0 ).result != Poller::Result::Type::Success ) {
        break;
      }
    }
  }

  return EXIT_SUCCESS;
}
