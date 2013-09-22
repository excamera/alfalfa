#ifndef FILE_DESCRIPTOR_HH
#define FILE_DESCRIPTOR_HH

#include <unistd.h>
#include <sys/stat.h>

#include "exception.hh"

class FileDescriptor
{
private:
  int fd_;

public:
  FileDescriptor( const int s_fd ) : fd_( s_fd ) {}

  ~FileDescriptor() { SystemCall( "close", close( fd_ ) ); }

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
};

#endif /* FILE_DESCRIPTOR_HH */
