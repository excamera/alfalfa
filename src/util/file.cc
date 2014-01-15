#include <fcntl.h>
#include <sys/mman.h>

#include "file.hh"
#include "exception.hh"

using namespace std;

File::File( const std::string & filename )
  : fd_( SystemCall( filename, open( filename.c_str(), O_RDONLY ) ) ),
    size_( fd_.size() ),
    buffer_( static_cast<uint8_t *>( mmap( nullptr, size_, PROT_READ, MAP_SHARED, fd_.num(), 0 ) ) ),
    chunk_( buffer_, size_ )
{
  if ( buffer_ == MAP_FAILED ) {
    throw Exception( "mmap" );
  }
}

File::~File()
{
  SystemCall( "munmap", munmap( buffer_, size_ ) );
}
