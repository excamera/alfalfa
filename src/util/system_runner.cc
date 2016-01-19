/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <vector>
#include <cassert>
#include <unistd.h>
#include <thread>
#include <exception>

#include "child_process.hh"
#include "exception.hh"

using namespace std;

int ezexec( const vector< string > & command, const bool path_search )
{
    if ( command.empty() ) {
        throw runtime_error( "ezexec: empty command" );
    }

    if ( geteuid() == 0 or getegid() == 0 ) {
        if ( environ ) {
            throw runtime_error( "BUG: root's environment not cleared" );
        }

        if ( path_search ) {
            throw runtime_error( "BUG: root should not search PATH" );
        }
    }

    /* copy the arguments to mutable structures */
    vector<char *> argv;
    vector< vector< char > > argv_data;

    for ( auto & x : command ) {
        vector<char> new_str;
        for ( auto & ch : x ) {
            new_str.push_back( ch );
        }
        new_str.push_back( 0 ); /* null-terminate */

        argv_data.push_back( new_str );
    }

    for ( auto & x : argv_data ) {
        argv.push_back( &x[ 0 ] );
    }
    argv.push_back( 0 ); /* null-terminate */

    SystemCall( path_search ? "execvpe" : "execve",
                (path_search ? execvpe : execve )( &argv[ 0 ][ 0 ], &argv[ 0 ], environ ) );
    throw runtime_error( "execve: failed" );
}

void run( const vector< string > & command )
{
    ChildProcess command_process( join( command ), [&] () {
            return ezexec( command );
        } );

    while ( !command_process.terminated() ) {
        command_process.wait();
    }

    if ( command_process.exit_status() != 0 ) {
        command_process.throw_exception();
    }
}
