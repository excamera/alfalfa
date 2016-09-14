/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef FILE_DESCRIPTOR_HH
#define FILE_DESCRIPTOR_HH

#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdio>

#include "exception.hh"
#include "chunk.hh"

class FileDescriptor
{
private:
  int fd_;
  bool eof_ { false };

public:
  // NOTE that the below constructor first seeks file to 0.
  FileDescriptor( FILE *file ) : fd_( (fseek(file, 0, SEEK_SET), fileno(file)) ) {}
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

  bool eof() { return eof_; }

  /* disallow copying or assigning */
  FileDescriptor( const FileDescriptor & other ) = delete;
  const FileDescriptor & operator=( const FileDescriptor & other ) = delete;

  /* allow moves */
  FileDescriptor( FileDescriptor && other )
    : fd_( other.fd_ ), eof_( other.eof_ )
  {
    // Need to make sure the old file descriptor doesn't try to
    // close fd_ when it is destructed
    other.fd_ = -1;
  }

  std::string::const_iterator write( const std::string::const_iterator & begin,
    const std::string::const_iterator & end )
  {
    if ( begin >= end ) {
      throw std::runtime_error( "nothing to write" );
    }

    ssize_t bytes_written = SystemCall( "write", ::write( fd_, &*begin, end - begin ) );

    if ( bytes_written == 0 ) {
      throw std::runtime_error( "write returned 0" );
    }

    return begin + bytes_written;
  }

  std::string::const_iterator write( const std::string & buffer, const bool write_all = true )
  {
    auto it = buffer.begin();

    do {
      it = write( it, buffer.end() );
    } while ( write_all and (it != buffer.end()) );

    return it;
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

  std::string pread( const off_t offset, const size_t length )
  {
    static const size_t BUFFER_SIZE = 1048576;
    char buffer[ BUFFER_SIZE ];

    std::string ret;

    while ( ret.length() < length ) {
      const size_t count = std::min( BUFFER_SIZE, length - ret.length() );
      ssize_t bytes_read_now = ::pread( fd_, buffer, count, offset + ret.length() );
      if ( bytes_read_now == 0 ) {
        // throw std::runtime_error( "pread: unexpected EOF" );
        eof_ = true;
      } else if ( bytes_read_now < 0 ) {
        throw unix_error( "pread" );
      }

      ret.append( buffer, bytes_read_now );
    }

    return ret;
  }

  std::string getline()
  {
    std::string ret;

    while ( true ) {
      std::string char_read = read( 1 );

      if ( eof() or char_read == "\n" ) {
        break;
      }

      ret.append( char_read );
    }

    return ret;
  }

  std::string read( const size_t limit )
  {
    static const size_t BUFFER_SIZE = 1048576;
    char buffer[ BUFFER_SIZE ];

    if ( eof() ) {
      throw std::runtime_error( "read() called after eof was set" );
    }

    ssize_t bytes_read = SystemCall( "read",
      ::read( fd_, buffer, std::min( BUFFER_SIZE, limit ) ) );

    if ( bytes_read == 0 ) {
      eof_ = true;
    }

    return std::string( buffer, bytes_read );
  }
};

#endif /* FILE_DESCRIPTOR_HH */
