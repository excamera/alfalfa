/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <algorithm>
#include <numeric>
#include "poller.hh"
#include "exception.hh"

using namespace std;
using namespace PollerShortNames;

void Poller::add_action( Poller::Action action )
{
    actions_.push_back( action );
    pollfds_.push_back( { action.fd.fd_num(), 0, 0 } );
}

unsigned int Poller::Action::service_count( void ) const
{
    return direction == Direction::In ? fd.read_count() : fd.write_count();
}

Poller::Result Poller::poll( const int & timeout_ms )
{
    assert( pollfds_.size() == actions_.size() );

    /* tell poll whether we care about each fd */
    for ( unsigned int i = 0; i < actions_.size(); i++ ) {
        assert( pollfds_.at( i ).fd == actions_.at( i ).fd.fd_num() );
        pollfds_.at( i ).events = (actions_.at( i ).active and actions_.at( i ).when_interested())
            ? actions_.at( i ).direction : 0;

        /* don't poll in on fds that have had EOF */
        if ( actions_.at( i ).direction == Direction::In
             and actions_.at( i ).fd.eof() ) {
            pollfds_.at( i ).events = 0;
        }
    }

    /* Quit if no member in pollfds_ has a non-zero direction */
    if ( not accumulate( pollfds_.begin(), pollfds_.end(), false,
                         [] ( bool acc, pollfd x ) { return acc or x.events; } ) ) {
        return Result::Type::Exit;
    }

    if ( 0 == SystemCall( "poll", ::poll( &pollfds_[ 0 ], pollfds_.size(), timeout_ms ) ) ) {
        return Result::Type::Timeout;
    }

    for ( unsigned int i = 0; i < pollfds_.size(); i++ ) {
        if ( pollfds_[ i ].revents & (POLLERR | POLLHUP | POLLNVAL) ) {
            //            throw Exception( "poll fd error" );
            return Result::Type::Exit;
        }

        if ( pollfds_[ i ].revents & pollfds_[ i ].events ) {
            /* we only want to call callback if revents includes
               the event we asked for */
            const auto count_before = actions_.at( i ).service_count();
            auto result = actions_.at( i ).callback();

            switch ( result.result ) {
            case ResultType::Exit:
                return Result( Result::Type::Exit, result.exit_status );
            case ResultType::Cancel:
                actions_.at( i ).active = false;
                break;
            case ResultType::Continue:
                break;
            }

            if ( count_before == actions_.at( i ).service_count() ) {
                throw runtime_error( "Poller: busy wait detected: callback did not read/write fd" );
            }
        }
    }

    return Result::Type::Success;
}
