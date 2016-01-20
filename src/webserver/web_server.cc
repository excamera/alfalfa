#include <string>
#include <iostream>
#include <climits>

#include "web_server.hh"
#include "temp_file.hh"
#include "exception.hh"
#include "system_runner.hh"

#include "config.h"

/* taken from mahimahi */

using namespace std;

WebServer::WebServer( const string & listen_address,
		      const string & video_directory,
		      const string & redirect_filename )
  : config_file_( "/tmp/alfalfa_apache_config" ),
    moved_away_( false )
{
  config_file_.write( string( "LoadModule authz_core_module " ) + MOD_AUTHZ_CORE + "\n" );
  config_file_.write( string( "LoadModule mpm_prefork_module " ) + MOD_MPM_PREFORK + "\n" );
  config_file_.write( "Mutex pthread\n" );
  config_file_.write( string( "LoadModule rewrite_module " ) + MOD_REWRITE + "\n" );
  config_file_.write( "RewriteEngine On\n" );
  config_file_.write( "RewriteRule ^(.*)$ /" + redirect_filename + "\n" );
  
  /* sanity-check directory name */
  for ( const char & c : video_directory ) {
    if ( (c == '"') or (c == 0) or (c == '\\') ) {
      throw runtime_error( "invalid character in video directory path" );
    }
  }

  /* get rooted path to directory */
  char resolved_path[ PATH_MAX ];

  if ( realpath( video_directory.c_str(), resolved_path ) == nullptr ) {
    throw unix_error( "realpath (" + video_directory + ")" );
  }

  config_file_.write( string( "DocumentRoot \"" ) + resolved_path + "\"\n" );

  config_file_.write( "PidFile /tmp/alfalfa_apache_pid." + to_string( getpid() ) + "." + to_string( random() ) + "\n" );

  config_file_.write( "ErrorLog /dev/null\n" );

  config_file_.write( "ServerName alfalfa\n" );

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
