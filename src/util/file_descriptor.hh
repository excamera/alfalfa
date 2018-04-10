/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#ifndef FILE_DESCRIPTOR_HH
#define FILE_DESCRIPTOR_HH

#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdio>
#include <cassert>

#include "exception.hh"
#include "chunk.hh"

static constexpr size_t BUFFER_SIZE = 1048576;

class FileDescriptor
{
private:
  int fd_;
  bool eof_ { false };

  unsigned int read_count_, write_count_;

protected:
  void register_read( void ) { read_count_++; }
  void register_write( void ) { write_count_++; }

public:
  // NOTE that the below constructor first seeks file to 0.
  FileDescriptor( FILE *file ) : fd_( (fseek(file, 0, SEEK_SET), fileno(file)) ),
                                 read_count_( 0 ), write_count_( 0 ) {}
  FileDescriptor( const int s_fd ) : fd_( s_fd ), read_count_( 0 ), write_count_( 0 ) {}

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

  const int & fd_num( void ) const { return fd_; }

  bool eof() { return eof_; }

  unsigned int read_count( void ) const { return read_count_; }
  unsigned int write_count( void ) const { return write_count_; }

  /* disallow copying or assigning */
  FileDescriptor( const FileDescriptor & other ) = delete;
  const FileDescriptor & operator=( const FileDescriptor & other ) = delete;

  /* allow moves */
  FileDescriptor( FileDescriptor && other )
    : fd_( other.fd_ ), eof_( other.eof_ ), read_count_( other.read_count_ ),
      write_count_( other.write_count_ )
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

    register_write();

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

    register_write();
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

  std::string read( const size_t limit = BUFFER_SIZE )
  {
    char buffer[ BUFFER_SIZE ];

    if ( eof() ) {
      throw std::runtime_error( "read() called after eof was set" );
    }

    ssize_t bytes_read = SystemCall( "read",
      ::read( fd_, buffer, std::min( BUFFER_SIZE, limit ) ) );

    if ( bytes_read == 0 ) {
      eof_ = true;
    }

    register_read();

    return std::string( buffer, bytes_read );
  }

  std::string read_exactly( const size_t length )
  {
    std::string ret;
    while ( ret.size() < length ) {
      ret.append( read( length - ret.size() ) );
      if ( eof() ) {
        throw std::runtime_error( "read_exactly: FileDescriptor reached EOF before reaching target" );
      }
    }

    assert( ret.size() == length );
    return ret;
  }
};

#endif /* FILE_DESCRIPTOR_HH */
