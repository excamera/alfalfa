#ifndef WEB_SERVER_HH
#define WEB_SERVER_HH

#include <string>

#include "temp_file.hh"

/* taken from mahimahi */

class WebServer
{
private:
  /* each apache instance needs unique configuration file, error/access logs, and pid file */
  TempFile config_file_;

  bool moved_away_;

public:
  WebServer( const std::string & listen_address,
	     const std::string & video_directory,
	     const std::string & redirect_filename );
  ~WebServer();

  /* ban copying */
  WebServer( const WebServer & other ) = delete;
  WebServer & operator=( const WebServer & other ) = delete;

  /* allow move constructor */
  WebServer( WebServer && other );

  /* ... but not move assignment operator */
  WebServer & operator=( WebServer && other ) = delete;
};

#endif
