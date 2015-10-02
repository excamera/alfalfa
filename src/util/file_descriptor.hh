#ifndef FILE_DESCRIPTOR_HH
#define FILE_DESCRIPTOR_HH

#include <unistd.h>
#include <sys/stat.h>

#include "exception.hh"
#include "chunk.hh"

class FileDescriptor
{
private:
  int fd_;

public:
  FileDescriptor( const int s_fd ) : fd_( s_fd ) {}

  ~FileDescriptor() 
  { 
    if ( fd_ >= 0 ) {
      SystemCall( "close", close( fd_ ) ); 
    }
  }

  uint64_t size( void ) const
  {
    struct stat file_info;
    SystemCall( "fstat", fstat( fd_, &file_info ) );
    return file_info.st_size;
  }

  const int & num( void ) const { return fd_; }

  /* disallow copying or assigning */
  FileDescriptor( const FileDescriptor & other ) = delete;
  const FileDescriptor & operator=( const FileDescriptor & other ) = delete;

  /* allow moves */
  FileDescriptor( FileDescriptor && other )
    : fd_( other.fd_ )
  {
    // Need to make sure the old file descriptor doesn't try to
    // close fd_ when it is destructed
    other.fd_ = -1;
  }

  void write( const Chunk & buffer )
  {
    Chunk amount_left_to_write = buffer;
    while ( amount_left_to_write.size() > 0 ) {
      ssize_t bytes_written = SystemCall( "write",
					  ::write( fd_, amount_left_to_write.buffer(), amount_left_to_write.size() ) );
      if ( bytes_written == 0 ) {
	throw internal_error( "write", "returned 0" );
      }
      amount_left_to_write = amount_left_to_write( bytes_written );
    }
  }
};

#endif /* FILE_DESCRIPTOR_HH */
