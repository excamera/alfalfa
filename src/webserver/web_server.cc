#include <string>
#include <iostream>

#include "web_server.hh"
#include "temp_file.hh"
#include "exception.hh"
#include "system_runner.hh"

#include "config.h"

/* taken from mahimahi */

using namespace std;

WebServer::WebServer( const string & listen_address,
		      const string & video_filename )
  : config_file_( "/tmp/alfalfa_apache_config" ),
    moved_away_( false )
{
  config_file_.write( string( "LoadModule mpm_prefork_module " ) + MOD_MPM_PREFORK + "\n" );
  config_file_.write( "Mutex pthread\n" );

  config_file_.write( string( "LoadModule rewrite_module " ) + MOD_REWRITE + "\n" );
  config_file_.write( "RewriteEngine On\n" );
  
  config_file_.write( "RewriteRule ^(.*)$ " );
  config_file_.write( video_filename + "\n" );
;

  config_file_.write( "PidFile /tmp/alfalfa_apache_pid." + to_string( getpid() ) + "." + to_string( random() ) + "\n" );

  config_file_.write( "ServerName alfalfa.\n" );

  config_file_.write( "ErrorLog /dev/null\n" );

  config_file_.write( "CustomLog /dev/null common\n" );

  config_file_.write( "User #" + to_string( getuid() ) + "\n" );

  config_file_.write( "Group #" + to_string( getgid() ) + "\n" );

  config_file_.write( "Listen " + listen_address );

  run( { APACHE2, "-f", config_file_.name(), "-k", "start" } );
}

WebServer::~WebServer()
{
  if ( moved_away_ ) { return; }

  try {
    run( { APACHE2, "-f", config_file_.name(), "-k", "graceful-stop" } );
  } catch ( const exception & e ) { /* don't throw from destructor */
    print_exception( "apache2 shutdown", e );
  }
}

WebServer::WebServer( WebServer && other )
  : config_file_( move( other.config_file_ ) ),
    moved_away_( false )
{
  other.moved_away_ = true;
}
