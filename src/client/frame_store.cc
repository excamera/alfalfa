#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "frame_store.hh"
#include "exception.hh"

using namespace std;

BackingStore::BackingStore()
  : file_( "alfalfa.local" ),
    file_size_( file_.fd().size() )
{
  if ( file_size_ ) {
    throw runtime_error( "BackingStore: unexpected nonzero file size" );
  }
}

size_t BackingStore::append( const Chunk & chunk )
{
  const size_t written_offset = file_size_;

  file_.fd().write( chunk );
  file_size_ += chunk.size();
  assert( file_.fd().size() == file_size_ );

  return written_offset;
}

void FrameStore::insert_frame( FrameInfo frame_info,
			       const Chunk & chunk )
{
  pending_.erase( frame_info.name() );
  
  if ( not local_frame_db_.has_frame_name( frame_info.name() ) ) {
    size_t offset = backing_store_.append( chunk );
    frame_info.set_offset( offset );

    cerr << "inserting " << frame_info.name().str() << " at " << frame_info.offset() << " len=" << frame_info.length() << "\n";
  
    local_frame_db_.insert( frame_info );
  }
}

bool FrameStore::has_frame( const FrameName & frame_name ) const
{
  return local_frame_db_.has_frame_name( frame_name );
}

string BackingStore::read( const size_t offset, const size_t length ) const
{
  if ( offset + length > file_size_ ) {
    throw std::out_of_range( "attempt to read beyond BackingStore" );
  }

  return file_.fd().pread( offset, length );
}

string FrameStore::coded_frame( const FrameName & frame_name ) const
{
  const FrameInfo & info = local_frame_db_.search_by_frame_name( frame_name );

  return backing_store_.read( info.offset(), info.length() );
}

void FrameStore::mark_frame_pending( const FrameName & name )
{
  pending_.insert( name );
}

bool FrameStore::is_frame_pending( const FrameName & name ) const
{
  return pending_.count( name );
}

bool FrameStore::is_anything_pending() const
{
  return not pending_.empty();
}
