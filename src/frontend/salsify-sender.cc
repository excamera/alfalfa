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

size_t target_size( uint32_t avg_delay, uint64_t last_acked, uint64_t last_sent )
{
  uint32_t max_delay = 100 * 1000; // 100 ms = 100,000 us
  cerr << "Packets in flight: " << last_sent - last_acked << "\n";
  cerr << "Avg inter-packet-arrival interval: " << avg_delay << "\n";
  cerr << "Imputed delay: " << avg_delay * (last_sent - last_acked) << " us\n";
  return 1400 * max( 0l, static_cast<int64_t>( max_delay / avg_delay - ( last_sent - last_acked ) ) );
}

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " QUANTIZER HOST PORT CONNECTION_ID" << endl;
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

  if ( argc != 5 ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  /* open the YUV4MPEG input */
  YUV4MPEGReader input { FileDescriptor( STDIN_FILENO ) };

  /* get quantizer argument */
  const unsigned int y_ac_qi = paranoid_atoi( argv[ 1 ] );

  /* get connection_id */
  const uint16_t connection_id = paranoid_atoi( argv[ 4 ] );

  /* construct Socket for outgoing datagrams */
  UDPSocket socket;
  socket.connect( Address( argv[ 2 ], argv[ 3 ] ) );
  socket.set_timestamps();

  /* average inter-packet delay, reported by receiver */
  uint32_t avg_delay = numeric_limits<uint32_t>::max();

  /* keep the number of fragments per frame */
  vector<uint64_t> cumulative_fpf;
  uint64_t last_acked = numeric_limits<uint64_t>::max();

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

      /*
      cerr << "ACK(frame: " << ack.frame_no() << ", "
           << "fragment: " << ack.fragment_no() << ", "
           << "avg delay: " << ack.avg_delay()
           << ")" << endl;
      */

      last_acked = ( ack.frame_no() > 0 )
                   ? ( cumulative_fpf[ ack.frame_no() - 1 ] + ack.fragment_no() )
                   : ack.fragment_no();

      return ResultType::Continue;
    },
    [&]() { return not input.fd().eof(); } )
  );

  /* construct the encoder */
  Encoder encoder { input.display_width(), input.display_height(), false /* two-pass */, REALTIME_QUALITY };

  /* keep the average encode time */
  EncodeTimeEWMA avg_encode_time;

  /* fetch frames from webcam */
  unsigned int frame_no = 0;
  poller.add_action( Poller::Action( input.fd(), Direction::In,
    [&]() -> Result {
      Optional<RasterHandle> raster = input.get_next_frame();

      if ( not raster.initialized() ) {
        return Result( ResultType::Exit, EXIT_FAILURE );
      }

      cerr << "Encoding frame #" << frame_no << "...";

      const auto encode_beginning = chrono::system_clock::now();
      vector<uint8_t> frame;

      if ( avg_delay == numeric_limits<uint32_t>::max() ) {
        frame = encoder.encode_with_quantizer( raster.get(), y_ac_qi );
      }
      else {
        size_t frame_size = target_size( avg_delay, last_acked, cumulative_fpf.back() );

        if ( frame_size <= 0 ) {
          cerr << "skipping frame." << endl;
          return ResultType::Continue;
        }

        cerr << "encoding with target size=" << frame_size << endl;
        frame = encoder.encode_with_target_size( raster.get(), frame_size );
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

      cumulative_fpf.push_back( ( frame_no > 0 ) ? ( cumulative_fpf[ frame_no - 1 ] + ff.fragments_in_this_frame() )
                                : ff.fragments_in_this_frame() );
      frame_no++;

      return ResultType::Continue;
    },
    [&]() { return not input.fd().eof(); } )
  );

  /* handle events */
  while ( true ) {
    const auto poll_result = poller.poll( -1 );
    if ( poll_result.result == Poller::Result::Type::Exit ) {
      return poll_result.exit_status;
    }
  }

  return EXIT_FAILURE;
}
