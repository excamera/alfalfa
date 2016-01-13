#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>

#include <iostream>
#include <fstream>
#include <string>

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include "exception.hh"
#include "alfalfa_video_server.hh"

using namespace std;
using namespace grpc;

int main( int argc, char const *argv[] )
{
  try
  {
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[ 0 ] << " <alf> <server-address>" << endl;
      return EX_USAGE;
    }

    string alf_path( argv[ 1 ] );
    string server_address( argv[ 2 ] );

    AlfalfaVideoServer service( alf_path );
    ServerBuilder builder;
    builder.AddListeningPort( server_address, grpc::InsecureServerCredentials() );
    builder.AddService( &server );

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Alfalfa server listening on " << server_address << endl;
    server->Wait();
  } catch (const exception &e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
