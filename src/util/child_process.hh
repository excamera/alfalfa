/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef CHILD_PROCESS_HH
#define CHILD_PROCESS_HH

#include <functional>
#include <unistd.h>
#include <cassert>
#include <csignal>

/* object-oriented wrapper for handling Unix child processes */

class ChildProcess
{
private:
    std::string name_;
    pid_t pid_;
    bool running_, terminated_;
    int exit_status_;
    bool died_on_signal_;
    int graceful_termination_signal_;

    bool moved_away_;

public:
    ChildProcess( const std::string & name,
                  std::function<int()> && child_procedure,
                  const int termination_signal = SIGHUP );

    bool waitable( void ) const; /* is process in a waitable state? */
    void wait( const bool nonblocking = false ); /* wait for process to change state */
    void signal( const int sig ); /* send signal */
    void resume( void ); /* send SIGCONT */

    const std::string & name( void ) const { assert( not moved_away_ ); return name_; }
    pid_t pid( void ) const { assert( not moved_away_ ); return pid_; }
    bool running( void ) const { assert( not moved_away_ ); return running_; }
    bool terminated( void ) const { assert( not moved_away_ ); return terminated_; }

    /* Return exit status or signal that killed process */
    bool died_on_signal( void ) const { assert( not moved_away_ ); assert( terminated_ ); return died_on_signal_; }
    int exit_status( void ) const { assert( not moved_away_ ); assert( terminated_ ); return exit_status_; }
    void throw_exception( void ) const;

    ~ChildProcess();

    /* ban copying */
    ChildProcess( const ChildProcess & other ) = delete;
    ChildProcess & operator=( const ChildProcess & other ) = delete;

    /* allow move constructor */
    ChildProcess( ChildProcess && other );

    /* ... but not move assignment operator */
    ChildProcess & operator=( ChildProcess && other ) = delete;
};

#endif /* CHILD_PROCESS_HH */
