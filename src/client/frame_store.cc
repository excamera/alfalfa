#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cassert>

#include "frame_store.hh"
#include "exception.hh"

using namespace std;

BackingStore::BackingStore()
  : file_( "/tmp/alfalfa.local" ),
    file_size_( file_.fd().size() )
{
  if ( file_size_ ) {
    throw runtime_error( "BackingStore: unexpected nonzero file size" );
  }
}

uint64_t BackingStore::append( const Chunk & chunk )
{
  const uint64_t written_offset = file_size_;

  file_.fd().write( chunk );
  file_size_ += chunk.size();
  assert( file_.fd().size() == file_size_ );

  return written_offset;
}

void FrameStore::insert_frame( const uint64_t global_offset,
			       const Chunk & chunk )
{
  pending_.erase( global_offset );

  if ( not has_frame( global_offset ) ) {
    const uint64_t local_offset = backing_store_.append( chunk );
    local_frame_db_.emplace( global_offset, make_pair( local_offset, chunk.size() ) );
  }
}

bool FrameStore::has_frame( const uint64_t global_offset ) const
{
  return ( local_frame_db_.find( global_offset ) != local_frame_db_.end() );
}

string BackingStore::read( const uint64_t offset, const size_t length ) const
{
  if ( offset + length > file_size_ ) {
    throw std::out_of_range( "attempt to read beyond BackingStore" );
  }

  return file_.fd().pread( offset, length );
}

string FrameStore::coded_frame( const uint64_t global_offset ) const
{
  const auto & local_info = local_frame_db_.find( global_offset );
  assert( local_info != local_frame_db_.end() );
  
  return backing_store_.read( local_info->second.first, local_info->second.second );
}

void FrameStore::mark_frame_pending( const uint64_t global_offset )
{
  pending_.emplace( global_offset );
}

bool FrameStore::is_frame_pending( const uint64_t global_offset ) const
{
  return ( pending_.find( global_offset ) != pending_.end() );
}

bool FrameStore::is_anything_pending() const
{
  return not pending_.empty();
}
