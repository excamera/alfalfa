/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef PACKET_HH
#define PACKET_HH

#include <vector>
#include <cassert>

#include "chunk.hh"
#include "socket.hh"
#include "exception.hh"

class Packet
{
private:
  bool valid_;

  uint16_t connection_id_;
  uint32_t frame_no_;
  uint16_t fragment_no_;
  uint16_t fragments_in_this_frame_;
  uint32_t time_to_next_;

  std::string payload_;

public:
  static constexpr size_t MAXIMUM_PAYLOAD = 1400;

  static std::string put_header_field( const uint16_t n );
  static std::string put_header_field( const uint32_t n );

  /* getters */
  bool valid() const { return valid_; }
  uint16_t connection_id() const { return connection_id_; }
  uint32_t frame_no() const { return frame_no_; }
  uint16_t fragment_no() const { return fragment_no_; }
  uint16_t fragments_in_this_frame() const { return fragments_in_this_frame_; }
  uint32_t time_to_next() const { return time_to_next_; }
  const std::string & payload() const { return payload_; }

  /* construct outgoing Packet */
  Packet( const std::vector<uint8_t> & whole_frame,
          const uint16_t connection_id,
          const uint32_t frame_no,
          const uint16_t fragment_no,
          const uint16_t time_to_next,
          size_t & next_fragment_start );

  /* construct incoming Packet */
  Packet( const Chunk & str );

  /* construct an empty, invalid packet */
  Packet();

  /* serialize a Packet */
  std::string to_string() const;

  void set_fragments_in_this_frame( const uint16_t x );
  void set_time_to_next( const uint32_t val ) { time_to_next_ = val; }
};

class FragmentedFrame
{
private:
  uint16_t connection_id_;
  uint32_t frame_no_;
  uint16_t fragments_in_this_frame_;

  std::vector<Packet> fragments_;

  uint32_t remaining_fragments_;

public:
  /* construct outgoing FragmentedFrame */
  FragmentedFrame( const uint16_t connection_id,
                   const uint32_t frame_no,
                   const uint32_t time_to_next_frame,
                   const std::vector<uint8_t> & whole_frame );

  /* construct incoming FragmentedFrame from a Packet */
  FragmentedFrame( const uint16_t connection_id,
                   const Packet & packet );

  void sanity_check( const Packet & packet ) const;

  /* read a new packet */
  void add_packet( const Packet & packet );

  /* send */
  void send( UDPSocket & socket );

  bool complete() const;

  /* getters */
  uint16_t connection_id() const { return connection_id_; }
  uint32_t frame_no() const { return frame_no_; }
  uint16_t fragments_in_this_frame() const { return fragments_in_this_frame_; }
  std::string frame() const;
  std::string partial_frame() const;

  /* delete copy-constructor and copy-assign operator */
  FragmentedFrame( const FragmentedFrame & other ) = delete;
  FragmentedFrame & operator=( const FragmentedFrame & other ) = delete;

  /* allow moving */
  FragmentedFrame( FragmentedFrame && other ) noexcept
    : connection_id_( other.connection_id_ ),
      frame_no_( other.frame_no_ ),
      fragments_in_this_frame_( other.fragments_in_this_frame_ ),
      fragments_( move( other.fragments_ ) ),
      remaining_fragments_( other.remaining_fragments_ )
  {}
};

class AckPacket
{
private:
  uint16_t connection_id_;
  uint32_t frame_no_;
  uint16_t fragment_no_;

public:
  AckPacket( const uint16_t connection_id, const uint32_t frame_no,
             const uint16_t fragment_no );

  AckPacket( const Chunk & str );

  std::string to_string();

  void sendto( UDPSocket & socket, const Address & addr );

  /* getters */
  uint16_t connection_id() const { return connection_id_; }
  uint32_t frame_no() const { return frame_no_; }
  uint16_t fragment_no() const { return fragment_no_; }
};

#endif /* PACKET_HH */
