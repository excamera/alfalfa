#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include "exception.hh"
#include "alfalfa_video_service.hh"
#include "web_server.hh"

using namespace std;
using namespace grpc;

int main( int argc, char const *argv[] )
{
  try
  {
    if ( argc != 5 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf> <server-address> <webserver-address> <url>" << endl;
      return EX_USAGE;
    }

    string alf_path( argv[ 1 ] );
    string server_address( argv[ 2 ] );
    string webserver_address( argv[ 3 ] );
    string url( argv[ 4 ] );

    AlfalfaVideoServiceImpl service( alf_path, url );
    ServerBuilder builder;

    cout << "Starting Apache... ";
    WebServer apache( webserver_address, alf_path, service.ivf_filename() );
    cout << "done.\n";
    
    builder.AddListeningPort( server_address, grpc::InsecureServerCredentials() );
    builder.RegisterService( &service );
    
    unique_ptr<Server> server( builder.BuildAndStart() );
    cout << "Alfalfa server listening on " << server_address << endl;

    if ( not isatty( STDIN_FILENO ) ) {
      char x;
      cin >> x;
    } else {
      /* we don't want the user to press Ctrl-C to quit,
	 so let them shut down server with any keystroke */
      /* (otherwise have to do more work to get gRPC server and
	 Apache to exit cleanly */

      /* get terminal driver configuration */
      termios terminal;
      SystemCall( "tcgetattr", tcgetattr( STDIN_FILENO, &terminal ) );

      /* put terminal driver into raw mode */
      termios raw_terminal = terminal;
      cfmakeraw( &raw_terminal );
      raw_terminal.c_oflag |= OPOST; /* have \n translated to \r\n on Linux */
      SystemCall( "tcsetattr", tcsetattr( STDIN_FILENO, TCSANOW, &raw_terminal ) );
    
      /* wait for one keystroke */
      cout << "Press any key to quit.\r\n";
      char x;
      SystemCall( "read", read( STDIN_FILENO, &x, 1 ) );

      /* restore terminal state before shutting down */
      SystemCall( "tcsetattr", tcsetattr( STDIN_FILENO, TCSANOW, &terminal ) );
    }
    
    cout << "Shutting down...\n";
  } catch (const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
