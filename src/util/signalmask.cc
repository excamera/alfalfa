/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <csignal>
#include <cstring>

#include "signalmask.hh"
#include "exception.hh"

using namespace std;

/* add given signals to the mask */
SignalMask::SignalMask( const initializer_list< int > signals )
    : mask_()
{
    SystemCall( "sigemptyset", sigemptyset( &mask_ ) );

    for ( const auto signal : signals ) {
        SystemCall( "sigaddset", sigaddset( &mask_, signal ) );
    }
}

/* get the current mask */
SignalMask SignalMask::current_mask( void )
{
    SignalMask mask = {};

    SystemCall( "sigprocmask", sigprocmask( SIG_BLOCK, nullptr, &mask.mask_ ) );

    return mask;
}

/* challenging to compare two sigset_t's for equality */
bool SignalMask::operator==( const SignalMask & other ) const
{
    for ( int signum = 0; signum < SIGRTMAX; signum++ ) {
        if ( sigismember( &mask_, signum ) != sigismember( &other.mask_, signum ) ) {
            return false;
        }
    }

    return true;
}

/* mask these signals from interrupting our process */
/* (because we'll use a signalfd instead to read them */
void SignalMask::set_as_mask( void ) const
{
    SystemCall( "sigprocmask", sigprocmask( SIG_SETMASK, &mask_, nullptr ) );
}
