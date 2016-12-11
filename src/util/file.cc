/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <fcntl.h>
#include <sys/mman.h>

#include "file.hh"
#include "exception.hh"

using namespace std;

File::File( const string & filename )
  : File( SystemCall( filename, open( filename.c_str(), O_RDONLY ) ) )
{ }

File::File( FileDescriptor && fd )
  : fd_( move( fd ) ),
    size_( fd_.size() ),
    mmap_region_( MMap_Region( size_, PROT_READ, MAP_SHARED, fd_.fd_num() ) ),
    chunk_( mmap_region_.addr(), size_ )
{ }

File::File( File && other )
  : fd_( move( other.fd_ ) ),
    size_( move( other.size_ ) ),
    mmap_region_( move( other.mmap_region_ ) ),
    chunk_( move( other.chunk_ ) )
{ }
