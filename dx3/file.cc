#include <fcntl.h>
#include <sys/mman.h>
#include <stdexcept>

#include "file.hh"
#include "exception.hh"

using namespace std;

File::File( const string & filename )
  : fd_( SystemCall( filename, open( filename.c_str(), O_RDONLY ) ) ),
    size_( fd_.size() ),
    buffer_( nullptr )
{
  /* map file into memory */
  buffer_ = static_cast<char *>( mmap( nullptr, size_, PROT_READ, MAP_SHARED, fd_.num(), 0 ) );
  if ( buffer_ == MAP_FAILED ) {
    throw Exception( "mmap" );
  }
}

File::~File()
{
  SystemCall( "munmap", munmap( buffer_, size_ ) );
}

Block File::block( const uint64_t & offset, const uint32_t & length ) const
{
  if ( offset + length > size() ) {
    throw out_of_range( "attempted to read past end of file" );
  }

  return Block( buffer_ + offset, length );
}
