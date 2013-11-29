#include <fcntl.h>
#include <sys/mman.h>

#include "file.hh"
#include "exception.hh"

using namespace std;

File::File( const std::string & filename )
  : fd_( SystemCall( filename, open( filename.c_str(), O_RDONLY ) ) ),
    chunk_( 0, 0 )
{
  uint64_t size = fd_.size();
  uint8_t *buffer = static_cast<uint8_t *>( mmap( nullptr, size, PROT_READ, MAP_SHARED, fd_.num(), 0 ) );
  if ( buffer == MAP_FAILED ) {
    throw Exception( "mmap" );
  }

  chunk_ = Chunk( buffer, size );
}

File::~File()
{
  SystemCall( "munmap", munmap( chunk_.mutable_buffer(), chunk_.size() ) );
}
