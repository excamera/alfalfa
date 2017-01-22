/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef PACER_HH
#define PACER_HH

#include <deque>
#include <chrono>
#include <string>

/* pace outgoing packets */
class Pacer
{
private:
  struct ScheduledPacket {
    std::chrono::system_clock::time_point when; /* scheduled outgoing time of packet */
    std::string what; /* serialized packet contents */
  };

  std::deque<ScheduledPacket> queue_ {};

public:
  int ms_until_due() const
  {
    if ( queue_.empty() ) {
      return 1000; /* could be infinite, but if there's a bug I'd rather we find it in the first second */
    }

    int millis = std::chrono::duration_cast<std::chrono::milliseconds>( queue_.front().when - std::chrono::system_clock::now() ).count();
    if ( millis < 0 ) {
      millis = 0;
    }

    return millis;
  }

  bool empty() const { return queue_.empty(); }
  void push( const std::string & payload, const int delay_microseconds )
  {
    if ( empty() ) {
      queue_.push_back( { std::chrono::system_clock::now(), payload } );
    } else {
      queue_.push_back( { queue_.back().when + std::chrono::microseconds( delay_microseconds ),
                          payload } );
    }
  }

  const std::string & front() const { return queue_.front().what; }
  void pop() { queue_.pop_front(); }
  size_t size() const { return queue_.size(); }
};

#endif /* PACER_HH */
