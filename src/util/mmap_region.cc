/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <sys/mman.h>

#include "mmap_region.hh"
#include "exception.hh"

using namespace std;

MMap_Region::MMap_Region( const size_t length, const int prot, const int flags, const int fd ) 
  : addr_( static_cast<uint8_t *>( mmap( nullptr, length, prot, flags, fd, 0 ) ) ), 
    length_( length )
{
  if ( addr_ == MAP_FAILED ) {
    throw unix_error( "mmap" );
  }
}

MMap_Region::~MMap_Region()
{
  if ( addr_ ) {
    SystemCall( "munmap", munmap( addr_, length_ ) );
  }
}

MMap_Region::MMap_Region( MMap_Region && other )
  : addr_( other.addr_ ),
    length_( other.length_ )
{
  other.addr_ = nullptr;
}
