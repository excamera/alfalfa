/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <limits>
#include <thread>
#include <future>
#include <algorithm>
#include <unordered_map>

#include "yuv4mpeg.hh"
#include "encoder.hh"
#include "socket.hh"
#include "packet.hh"
#include "poller.hh"
#include "socketpair.hh"
#include "exception.hh"
#include "finally.hh"
#include "paranoid.hh"

using namespace std;
using namespace std::chrono;
using namespace PollerShortNames;

class AverageEncodingTime
{
private:
  static constexpr double ALPHA = 0.1;

  double value_ { -1.0 };
  microseconds last_update_{ 0 };

public:
  void add( const microseconds timestamp_us )
  {
    assert( timestamp_us >= last_update_ );

    if ( value_ < 0 ) {
      value_ = 0;
    }
    else if ( timestamp_us - last_update_ > 1s /* 1 seconds */ ) {
      value_ = 0;
    }
    else {
      double new_value = max( 0l, duration_cast<microseconds>( timestamp_us - last_update_ ).count() );
      value_ = ALPHA * new_value + ( 1 - ALPHA ) * value_;
    }

    last_update_ = timestamp_us;
  }

  uint32_t int_value() const { return static_cast<uint32_t>( value_ ); }
};

struct EncodeJob
{
  uint32_t frame_no;

  RasterHandle raster;

  Encoder encoder;
  EncoderMode mode;

  uint8_t y_ac_qi;
  size_t target_size;

  EncodeJob( const uint32_t frame_no, RasterHandle raster, const Encoder & encoder,
             const EncoderMode mode, const uint8_t y_ac_qi, const size_t target_size )
    : frame_no( frame_no ), raster( raster ), encoder( encoder ),
      mode( mode ), y_ac_qi( y_ac_qi ), target_size( target_size )
  {}
};

struct EncodeOutput
{
  Encoder encoder;
  vector<uint8_t> frame;
  uint32_t source_minihash;
  milliseconds encode_time;

  EncodeOutput( Encoder && encoder, vector<uint8_t> && frame,
                const uint32_t source_minihash, const milliseconds encode_time )
    : encoder( move( encoder ) ), frame( move( frame ) ),
      source_minihash( source_minihash ), encode_time( encode_time )
  {}
};

EncodeOutput do_encode_job( EncodeJob && encode_job )
{
  vector<uint8_t> output;

  uint32_t source_minihash = encode_job.encoder.minihash();

  const auto encode_beginning = system_clock::now();

  switch ( encode_job.mode ) {
  case CONSTANT_QUANTIZER:
    output = encode_job.encoder.encode_with_quantizer( encode_job.raster.get(),
                                                       encode_job.y_ac_qi );
    break;

  case TARGET_FRAME_SIZE:
    output = encode_job.encoder.encode_with_target_size( encode_job.raster.get(),
                                                         encode_job.target_size );
    break;

  default:
    throw runtime_error( "unsupported encoding mode." );
  }

  const auto encode_ending = system_clock::now();
  const auto ms_elapsed = duration_cast<milliseconds>( encode_ending - encode_beginning );

  return { move( encode_job.encoder ), move( output ), source_minihash, ms_elapsed };
}

size_t target_size( uint32_t avg_delay, const uint64_t last_acked, const uint64_t last_sent )
{
  uint32_t max_delay = 100 * 1000; // 100 ms = 100,000 us

  if ( avg_delay == 0 ) { avg_delay = 1; }

  /* cerr << "Packets in flight: " << last_sent - last_acked << "\n";
  cerr << "Avg inter-packet-arrival interval: " << avg_delay << "\n";
  cerr << "Imputed delay: " << avg_delay * (last_sent - last_acked) << " us\n"; */

  return 1400 * max( 0l, static_cast<int64_t>( max_delay / avg_delay - ( last_sent - last_acked ) ) );
}

void usage( const char *argv0 )
{
  cerr << "Usage: " << argv0 << " QUANTIZER HOST PORT CONNECTION_ID" << endl;
}

uint64_t ack_seq_no( const AckPacket & ack,
                     const vector<uint64_t> & cumulative_fpf )
{
  return ( ack.frame_no() > 0 )
       ? ( cumulative_fpf[ ack.frame_no() - 1 ] + ack.fragment_no() )
       : ack.fragment_no();
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
  const unsigned int y_ac_qi = paranoid::stoul( argv[ 1 ] );

  /* get connection_id */
  const uint16_t connection_id = paranoid::stoul( argv[ 4 ] );

  /* construct Socket for outgoing datagrams */
  UDPSocket socket;
  socket.connect( Address( argv[ 2 ], argv[ 3 ] ) );
  socket.set_timestamps();

  /* average inter-packet delay, reported by receiver */
  uint32_t avg_delay = numeric_limits<uint32_t>::max();

  /* keep the number of fragments per frame */
  vector<uint64_t> cumulative_fpf;
  uint64_t last_acked = numeric_limits<uint64_t>::max();

  /* maximum number of frames to be skipped in a row */
  const size_t MAX_SKIPPED = 5;
  size_t skipped_count = 0;

  /* frame rate */
  static const int MS_PER_SECOND = 1000;
  uint8_t min_fps = 6;
  milliseconds max_time_per_frame { MS_PER_SECOND / min_fps };

  /* construct the encoder */
  Encoder base_encoder { input.display_width(), input.display_height(),
                         false /* two-pass */, REALTIME_QUALITY };

  const uint32_t initial_state = base_encoder.minihash();

  /* encoded frame index */
  unsigned int frame_no = 0;

  /* last raster recieved on the input */
  unsigned int input_raster_count = 0;

  /* latest raster that is received from the input */
  Optional<RasterHandle> last_raster;

  /* where we keep the outputs of parallel encoding jobs */
  vector<EncodeJob> encode_jobs;
  vector<future<EncodeOutput>> encode_outputs;

  /* keep the moving average of encoding times */
  AverageEncodingTime avg_encoding_time;

  /* decoder hash => encoder object */
  deque<uint32_t> encoder_states;
  unordered_map<uint32_t, Encoder> encoders { { initial_state, base_encoder } };

  /* latest state of the receiver, based on ack packets */
  Optional<uint32_t> receiver_last_acked_state;
  Optional<uint32_t> receiver_assumed_state;
  deque<uint32_t> receiver_complete_states;

  auto encode_end_pipe = UnixDomainSocket::make_pair();

  Poller poller;

  /* fetch frames from webcam */
  poller.add_action( Poller::Action( input.fd(), Direction::In,
    [&]() -> Result {
      last_raster = input.get_next_frame();
      input_raster_count++;

      if ( not last_raster.initialized() ) {
        return { ResultType::Exit, EXIT_FAILURE };
      }

      if ( encode_jobs.size() > 0 ) {
        /* a frame is being encoded now */
        return ResultType::Continue;
      }

      /* let's cleanup the stored encoders based on the lastest ack */
      if ( receiver_last_acked_state.initialized() and
           encoders.count( receiver_last_acked_state.get() ) ) {
        // cleaning up
        auto it = encoder_states.begin();

        while ( it != encoder_states.end() ) {
          if ( *it != receiver_last_acked_state.get() ) {
            encoders.erase( *it );
            it++;
          }
          else {
            break;
          }
        }

        encoder_states.erase( encoder_states.begin(), it );
      }

      RasterHandle raster = last_raster.get();
      const auto encode_deadline = system_clock::now() + max_time_per_frame;

      cerr << "Preparing encoding jobs for frame #" << frame_no << "." << endl;

      uint32_t selected_source_hash = initial_state;

      /* reason about the state of the receiver based on ack messages
       * this is the logic that decides which encoder to use. for example,
       * if the packet loss is huge, we can always select an encoder with a sure
       * state. */
      if ( not receiver_last_acked_state.initialized() ) {
        if ( not receiver_assumed_state.initialized() ) {
          /* okay, let's just encode as a keyframe */
          selected_source_hash = initial_state;
        }
        else {
          /* we assume that the receiver is in a right state */
          selected_source_hash = receiver_assumed_state.get();
        }
      }
      else {
        if ( encoders.count( receiver_last_acked_state.get() ) == 0 ) {
          /* it seems that the receiver is in an invalid state */
          if( receiver_complete_states.size() == 0 ) {
            /* and the receiver doesn't have any other states, other than the default state */
            selected_source_hash = initial_state;
          }
          else {
            /* the receiver has at least one stored state, let's use it */
            selected_source_hash = receiver_complete_states.back();
          }
        }
        else {
          /* we assume that the receiver is in a right state */
          selected_source_hash = receiver_assumed_state.get();
        }
      }
      /* end of encoder selection logic */

      Encoder & encoder = encoders.at( selected_source_hash );

      if ( avg_delay == numeric_limits<uint32_t>::max() ) {
        encode_jobs.emplace_back( frame_no, raster, encoder, CONSTANT_QUANTIZER, y_ac_qi, 0 );
      }
      else {
        size_t frame_size = target_size( avg_delay, last_acked, cumulative_fpf.back() );

        if ( frame_size <= 0 and skipped_count < MAX_SKIPPED ) {
          skipped_count++;
          cerr << "skipping frame." << endl;
          return ResultType::Continue;
        }
        else if ( frame_size == 0 ) {
          cerr << "too many skipped frames, let's send one with a low quality." << endl;
          encode_jobs.emplace_back( frame_no, raster, encoder, TARGET_FRAME_SIZE, 0, 1400 );
        }
        else {
          encode_jobs.emplace_back( frame_no, raster, encoder, TARGET_FRAME_SIZE, 0, frame_size );
        }
      }

      // this thread will spawn all the encoding jobs and will wait on the results
      thread(
        [&encode_jobs, &encode_outputs, &encode_end_pipe, encode_deadline]()
        {
          const auto encode_beginning = system_clock::now();

          encode_outputs.clear();
          encode_outputs.reserve( encode_jobs.size() );

          for ( auto & job : encode_jobs ) {
            encode_outputs.push_back( async( launch::async, do_encode_job, move( job ) ) );
          }

          for ( auto & future_res : encode_outputs ) {
            future_res.wait_until( encode_deadline );
          }

          const auto encode_ending = system_clock::now();
          const auto ms_elapsed = duration_cast<milliseconds>( encode_ending - encode_beginning );

          cerr << "Encoding done (time=" << ms_elapsed.count() << " ms)." << endl;

          encode_end_pipe.first.write( "1" );
        }
      ).detach();

      cerr << "Running " << encode_jobs.size() << " encoding job(s)." << endl;

      return ResultType::Continue;
    },
    [&]() { return not input.fd().eof(); } )
  );

  poller.add_action( Poller::Action( encode_end_pipe.second, Direction::In,
    [&]()
    {
      /* whatever happens, encode_jobs will be empty after this block is done. */
      auto _ = finally( [&]() { encode_jobs.clear(); } );

      encode_end_pipe.second.read();

      if ( not any_of( encode_outputs.cbegin(), encode_outputs.cend(),
                       [&]( const future<EncodeOutput> & o ) { return o.valid(); } ) ) {
        // no encoding job has ended in time
        return ResultType::Continue;
      }

      /* what is the current capacity of the network,
         now that the encoding is done? */
      size_t frame_size = numeric_limits<size_t>::max();

      if ( avg_delay != numeric_limits<uint32_t>::max() ) {
        frame_size = target_size( avg_delay, last_acked, cumulative_fpf.back() );
      }

      size_t best_output_index = numeric_limits<size_t>::max();
      size_t best_size_diff = numeric_limits<size_t>::max();

      vector<EncodeOutput> good_outputs;

      for ( auto & out_future : encode_outputs ) {
        if ( out_future.valid() ) {
          good_outputs.push_back( move( out_future.get() ) );
        }
      }

      /* choose the best based on the current capacity */
      for ( size_t i = 0; i < good_outputs.size(); i++ ) {
        if ( good_outputs[ i ].frame.size() <= frame_size ) {
          if ( frame_size - good_outputs[ i ].frame.size() < best_size_diff ) {
            best_size_diff = frame_size - good_outputs[ i ].frame.size();
            best_output_index = i;
          }
        }
      }

      if ( best_output_index == numeric_limits<size_t>::max() ) {
        best_output_index = good_outputs.size() - 1;
      }

      auto output = move( good_outputs[ best_output_index ] );

      uint32_t target_minihash = output.encoder.minihash();

      cerr << "Sending frame #" << frame_no << " (size=" << output.frame.size() << " bytes, "
           << "source_hash=" << output.source_minihash << ", target_hash="
           << target_minihash << ")...";

      FragmentedFrame ff { connection_id, output.source_minihash, frame_no,
                           avg_encoding_time.int_value(), output.frame };
      ff.send( socket );

      cerr << "done." << endl;

      cumulative_fpf.push_back( ( frame_no > 0 )
                                ? ( cumulative_fpf[ frame_no - 1 ] + ff.fragments_in_this_frame() )
                                : ff.fragments_in_this_frame() );

      /* now we assume that the receiver will successfully get this */
      receiver_assumed_state.reset( target_minihash );

      encoders.insert( make_pair( target_minihash, move( output.encoder ) ) );
      encoder_states.push_back( target_minihash );

      skipped_count = 0;
      frame_no++;

      avg_encoding_time.add( duration_cast<microseconds>( system_clock::now().time_since_epoch() ) );

      return ResultType::Continue;
    },
    [&]() { return not input.fd().eof(); } )
  );

  poller.add_action( Poller::Action( socket, Direction::In,
    [&]()
    {
      auto packet = socket.recv();
      AckPacket ack( packet.payload );

      if ( ack.connection_id() != connection_id ) {
        /* this is not an ack for this session! */
        return ResultType::Continue;
      }

      uint64_t this_ack_seq = ack_seq_no( ack, cumulative_fpf );

      if ( last_acked != numeric_limits<uint64_t>::max() and
           this_ack_seq < last_acked ) {
        /* we have already received an ACK newer than this */
        return ResultType::Continue;
      }

      last_acked = this_ack_seq;
      avg_delay = ack.avg_delay();
      receiver_last_acked_state.reset( ack.current_state() );
      receiver_complete_states = move( ack.complete_states() );

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
