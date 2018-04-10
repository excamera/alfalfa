/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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

#ifndef POLLER_HH
#define POLLER_HH

#include <functional>
#include <vector>
#include <cassert>

#include <poll.h>

#include "file_descriptor.hh"

class Poller
{
public:
    struct Action
    {
        struct Result
        {
            enum class Type { Continue, Exit, Cancel } result;
            unsigned int exit_status;
            Result( const Type & s_result = Type::Continue, const unsigned int & s_status = EXIT_SUCCESS )
                : result( s_result ), exit_status( s_status ) {}
        };

        typedef std::function<Result(void)> CallbackType;

        FileDescriptor & fd;
        enum PollDirection : short { In = POLLIN, Out = POLLOUT } direction;
        CallbackType callback;
        std::function<bool(void)> when_interested;
        bool active;

        Action( FileDescriptor & s_fd,
                const PollDirection & s_direction,
                const CallbackType & s_callback,
                const std::function<bool(void)> & s_when_interested = [] () { return true; } )
            : fd( s_fd ), direction( s_direction ), callback( s_callback ),
              when_interested( s_when_interested ), active( true ) {}

        unsigned int service_count( void ) const;
    };

private:
    std::vector< Action > actions_;
    std::vector< pollfd > pollfds_;

public:
    struct Result
    {
        enum class Type { Success, Timeout, Exit } result;
        unsigned int exit_status;
        Result( const Type & s_result, const unsigned int & s_status = EXIT_SUCCESS )
            : result( s_result ), exit_status( s_status ) {}
    };

    Poller() : actions_(), pollfds_() {}
    void add_action( Action action );
    Result poll( const int & timeout_ms );
};

namespace PollerShortNames {
    typedef Poller::Action::Result Result;
    typedef Poller::Action::Result::Type ResultType;
    typedef Poller::Action::PollDirection Direction;
}

#endif
