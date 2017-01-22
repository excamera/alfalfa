/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <getopt.h>

#include <cstdlib>
#include <random>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <queue>
#include <deque>
#include <thread>
#include <condition_variable>
#include <future>

#include "socket.hh"
#include "packet.hh"
#include "poller.hh"
#include "optional.hh"
#include "player.hh"
#include "display.hh"
#include "paranoid.hh"

using namespace std;
using namespace PollerShortNames;

class AverageInterPacketDelay
{
private:
  static constexpr double ALPHA = 0.1;

  double value_ { -1.0 };
  uint64_t last_update_{ 0 };

public:
  void add( const uint64_t timestamp_us, const int32_t grace )
  {
    assert( timestamp_us >= last_update_ );

    if ( value_ < 0 ) {
      value_ = 0;
    }
    // else if ( timestamp_us - last_update_ > 0.2 * 1000 * 1000 /* 0.2 seconds */ ) {
    //   value_ /= 4;
    // }
    else {
      double new_value = max( 0l, static_cast<int64_t>( timestamp_us - last_update_ - grace ) );
      value_ = ALPHA * new_value + ( 1 - ALPHA ) * value_;
    }

    last_update_ = timestamp_us;
  }

  uint32_t int_value() const { return static_cast<uint32_t>( value_ ); }
};

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " [-f, --fullscreen] PORT WIDTH HEIGHT" << endl;
}

uint16_t ezrand()
{
  random_device rd;
  uniform_int_distribution<uint16_t> ud;

  return ud( rd );
}

queue<RasterHandle> display_queue;
mutex mtx;
condition_variable cv;

void display_task( const VP8Raster & example_raster, bool fullscreen )
{
  VideoDisplay display { example_raster, fullscreen };

  while( true ) {
    unique_lock<mutex> lock( mtx );
    cv.wait( lock, []() { return not display_queue.empty(); } );

    while( not display_queue.empty() ) {
      display.draw( display_queue.front() );
      display_queue.pop();
    }
  }
}

void enqueue_frame( FramePlayer & player, const Chunk & frame )
{
  if ( frame.size() == 0 ) {
    return;
  }

  const Optional<RasterHandle> raster = player.decode( frame );

  async( launch::async,
    [&raster]()
    {
      if ( raster.initialized() ) {
        lock_guard<mutex> lock( mtx );
        display_queue.push( raster.get() );
        cv.notify_all();
      }
    }
  );
}

int main( int argc, char *argv[] )
{
  /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  /* fullscreen player */
  bool fullscreen = false;

  const option command_line_options[] = {
    { "fullscreen", no_argument, nullptr, 'f' },
    { 0, 0, 0, 0 }
  };

  while ( true ) {
    const int opt = getopt_long( argc, argv, "f", command_line_options, nullptr );

    if ( opt == -1 ) {
      break;
    }

    switch ( opt ) {
    case 'f':
      fullscreen = true;
      break;

    default:
      usage( argv[ 0 ] );
      return EXIT_FAILURE;
    }
  }

  if ( optind + 2 >= argc ) {
    usage( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  /* choose a random connection_id */
  const uint16_t connection_id = 1337; // ezrand();
  cerr << "Connection ID: " << connection_id << endl;

  /* construct Socket for incoming  datagrams */
  UDPSocket socket;
  socket.bind( Address( "0", argv[ optind ] ) );
  socket.set_timestamps();

  /* construct FramePlayer */
  FramePlayer player( paranoid::stoul( argv[ optind + 1 ] ), paranoid::stoul( argv[ optind + 2 ] ) );
  player.set_error_concealment( true );

  /* construct display thread */
  thread( [&player, fullscreen]() { display_task( player.example_raster(), fullscreen ); } ).detach();

  /* frame no => FragmentedFrame; used when receiving packets out of order */
  unordered_map<size_t, FragmentedFrame> fragmented_frames;
  size_t next_frame_no = 0;

  /* EWMA */
  AverageInterPacketDelay avg_delay;

  /* decoder states */
  uint32_t current_state = player.current_decoder().get_hash().hash();
  const uint32_t initial_state = current_state;
  deque<uint32_t> complete_states;
  unordered_map<uint32_t, Decoder> decoders { { current_state, player.current_decoder() } };

  Poller poller;
  poller.add_action( Poller::Action( socket, Direction::In,
    [&]()
    {
      /* wait for next UDP datagram */
      const auto new_fragment = socket.recv();

      /* parse into Packet */
      const Packet packet { new_fragment.payload };

      if ( packet.frame_no() < next_frame_no ) {
        /* we're not interested in this anymore */
        return ResultType::Continue;
      }
      else if ( packet.frame_no() > next_frame_no ) {
        /* current frame is not finished yet, but we just received a packet
           for the next frame, so here we just encode the partial frame and
           display it and move on to the next frame */
        cerr << "got a packet for frame #" << packet.frame_no()
             << ", display previous frame(s)." << endl;

        for ( size_t i = next_frame_no; i < packet.frame_no(); i++ ) {
          if ( fragmented_frames.count( i ) == 0 ) continue;

          enqueue_frame( player, fragmented_frames.at( i ).partial_frame() );
          fragmented_frames.erase( i );
        }

        next_frame_no = packet.frame_no();
        current_state = player.current_decoder().minihash();
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
        auto & fragment = fragmented_frames.at( next_frame_no );

        uint32_t expected_source_state = fragment.source_state();

        if ( current_state != expected_source_state ) {
          if ( decoders.count( expected_source_state ) ) {
            /* we have this state! let's load it */
            player.set_decoder( decoders.at( expected_source_state ) );
            current_state = expected_source_state;
          }
        }

        if ( current_state == expected_source_state and
             expected_source_state != initial_state ) {
          /* sender won't refer to any decoder older than this, so let's get
             rid of them */

          auto it = complete_states.begin();

          for ( ; it != complete_states.end(); it++ ) {
            if ( *it != expected_source_state ) {
              decoders.erase( *it );
            }
            else {
              break;
            }
          }

          assert( it != complete_states.end() );
          complete_states.erase( complete_states.begin(), it );
        }

        // here we apply the frame
        enqueue_frame( player, fragment.frame() );

        // state "after" applying the frame
        current_state = player.current_decoder().minihash();

        if ( current_state == fragment.target_state() ) {
          /* this is a full state. let's save it */
          decoders.insert( make_pair( current_state, player.current_decoder() ) );
          complete_states.push_back( current_state );
        }

        fragmented_frames.erase( next_frame_no );
        next_frame_no++;
      }

      avg_delay.add( new_fragment.timestamp_us, packet.time_since_last() );

      AckPacket( connection_id, packet.frame_no(), packet.fragment_no(),
                 avg_delay.int_value(), current_state,
                 complete_states ).sendto( socket, new_fragment.source_address );

      return ResultType::Continue;
    },
    [&]() { return not socket.eof(); } )
  );

  /* handle events */
  while ( true ) {
    const auto poll_result = poller.poll( -1 );
    if ( poll_result.result == Poller::Result::Type::Exit ) {
      return poll_result.exit_status;
    }
  }

  return EXIT_SUCCESS;
}
