/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <sys/types.h>
#include <sys/wait.h>
#include <cassert>
#include <cstdlib>
#include <sys/syscall.h>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "child_process.hh"
#include "exception.hh"
#include "signalmask.hh"

using namespace std;

template <typename T> void zero( T & x ) { memset( &x, 0, sizeof( x ) ); }

int do_fork()
{
    /* Verify that process is single-threaded before forking */
    {
        struct stat my_stat;
        SystemCall( "stat", stat( "/proc/self/task", &my_stat ) );

        if ( my_stat.st_nlink != 3 ) {
            throw runtime_error( "ChildProcess constructed in multi-threaded program" );
        }
    }

    return SystemCall( "fork", fork() );
}

/* start up a child process running the supplied lambda */
/* the return value of the lambda is the child's exit status */
ChildProcess::ChildProcess( const string & name,
                            function<int()> && child_procedure,
                            const int termination_signal )
    : name_( name ),
      pid_( do_fork() ),
      running_( true ),
      terminated_( false ),
      exit_status_(),
      died_on_signal_( false ),
      graceful_termination_signal_( termination_signal ),
      moved_away_( false )
{
    if ( pid_ == 0 ) { /* child */
        try {
            SignalMask( {} ).set_as_mask();
            _exit( child_procedure() );
        } catch ( const exception & e ) {
            print_exception( name_.c_str(), e );
            _exit( EXIT_FAILURE );
        }
    }
}

/* is process in a waitable state? */
bool ChildProcess::waitable( void ) const
{
    assert( !moved_away_ );
    assert( !terminated_ );

    siginfo_t infop;
    zero( infop );
    SystemCall( "waitid", waitid( P_PID, pid_, &infop,
                                  WEXITED | WSTOPPED | WCONTINUED | WNOHANG | WNOWAIT ) );

    if ( infop.si_pid == 0 ) {
        return false;
    } else if ( infop.si_pid == pid_ ) {
        return true;
    } else {
        throw runtime_error( "waitid: unexpected value in siginfo_t si_pid field (not 0 or pid)" );
    }
}

/* wait for process to change state */
void ChildProcess::wait( const bool nonblocking )
{
    assert( !moved_away_ );
    assert( !terminated_ );

    siginfo_t infop;
    zero( infop );
    SystemCall( "waitid", waitid( P_PID, pid_, &infop,
                                  WEXITED | WSTOPPED | WCONTINUED | (nonblocking ? WNOHANG : 0) ) );

    if ( nonblocking and (infop.si_pid == 0) ) {
        throw runtime_error( "nonblocking wait: process was not waitable" );
    }

    if ( infop.si_pid != pid_ ) {
        throw runtime_error( "waitid: unexpected value in siginfo_t si_pid field" );
    }

    if ( infop.si_signo != SIGCHLD ) {
        throw runtime_error( "waitid: unexpected value in siginfo_t si_signo field (not SIGCHLD)" );
    }

    /* how did the process change state? */
    switch ( infop.si_code ) {
    case CLD_EXITED:
        terminated_ = true;
        exit_status_ = infop.si_status;
        break;
    case CLD_KILLED:
    case CLD_DUMPED:
        terminated_ = true;
        exit_status_ = infop.si_status;
        died_on_signal_ = true;
        break;
    case CLD_STOPPED:
        running_ = false;
        break;
    case CLD_CONTINUED:
        running_ = true;
        break;
    default:
        throw runtime_error( "waitid: unexpected siginfo_t si_code" );
    }
}

/* if child process was suspended, resume it */
void ChildProcess::resume( void )
{
    assert( !moved_away_ );

    if ( !running_ ) {
        signal( SIGCONT );
    }
}

/* send a signal to the child process */
void ChildProcess::signal( const int sig )
{
    assert( !moved_away_ );

    if ( !terminated_ ) {
        SystemCall( "kill", kill( pid_, sig ) );
    }
}

ChildProcess::~ChildProcess()
{
    if ( moved_away_ ) { return; }

    try {
        while ( !terminated_ ) {
            resume();
            signal( graceful_termination_signal_ );
            wait();
        }
    } catch ( const exception & e ) {
        print_exception( name_.c_str(), e );
    }
}

/* move constructor */
ChildProcess::ChildProcess( ChildProcess && other )
    : name_( other.name_ ),
      pid_( other.pid_ ),
      running_( other.running_ ),
      terminated_( other.terminated_ ),
      exit_status_( other.exit_status_ ),
      died_on_signal_( other.died_on_signal_ ),
      graceful_termination_signal_( other.graceful_termination_signal_ ),
      moved_away_( other.moved_away_ )
{
    assert( !other.moved_away_ );

    other.moved_away_ = true;
}

void ChildProcess::throw_exception( void ) const
{
    throw runtime_error( "`" + name() + "': process "
                         + (died_on_signal()
                            ? string("died on signal ")
                            : string("exited with failure status "))
                         + to_string( exit_status() ) );
}
