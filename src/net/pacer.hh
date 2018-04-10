/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

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
