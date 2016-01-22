#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "frame_store.hh"
#include "exception.hh"

using namespace std;

BackingStore::BackingStore( const string & filename )
  : fd_( SystemCall( filename,
		     open( filename.c_str(),
			   O_RDWR | O_CREAT | O_TRUNC,
			   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ) ),
    file_size_( fd_.size() )
{
  if ( file_size_ ) {
    throw runtime_error( "BackingStore: unexpected nonzero file size" );
  }
}

size_t BackingStore::append( const Chunk & chunk )
{
  const size_t written_offset = file_size_;

  fd_.write( chunk );
  file_size_ += chunk.size();
  assert( fd_.size() == file_size_ );

  return written_offset;
}

FrameStore::FrameStore( const string & filename )
  : local_frame_db_(),
    backing_store_( filename )
{}

void FrameStore::insert_frame( FrameInfo frame_info,
			       const Chunk & chunk )
{
  if ( not local_frame_db_.has_frame_name( frame_info.name() ) ) {
    size_t offset = backing_store_.append( chunk );
    frame_info.set_offset( offset );
  }
  
  local_frame_db_.insert( frame_info );
}

bool FrameStore::has_frame( const FrameName & frame_name ) const
{
  return local_frame_db_.has_frame_name( frame_name );
}

string BackingStore::read( const size_t offset, const size_t length ) const
{
  if ( offset + length >= file_size_ ) {
    throw std::out_of_range( "attempt to read beyond BackingStore" );
  }

  string ret;

  static const size_t BUFFER_SIZE = 1048576;
  char buffer[ BUFFER_SIZE ];

  while ( ret.length() < length ) {
    const size_t count = min( BUFFER_SIZE, length - ret.length() );
    ssize_t bytes_read_now = pread( fd_.num(), buffer, count, offset + ret.length() );
    if ( bytes_read_now == 0 ) {
      throw runtime_error( "pread: unexpected EOF" );
    } else if ( bytes_read_now < 0 ) {
      throw unix_error( "pread" );
    }

    ret.append( buffer, bytes_read_now );
  }

  return ret; 
}

string FrameStore::coded_frame( const FrameName & frame_name ) const
{
  const FrameInfo & info = local_frame_db_.search_by_frame_name( frame_name );

  return backing_store_.read( info.offset(), info.length() );
}
