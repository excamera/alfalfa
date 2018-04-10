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

#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "ivf_writer.hh"
#include "safe_array.hh"
#include "mmap_region.hh"

using namespace std;

template <typename T> void zero( T & x ) { memset( &x, 0, sizeof( x ) ); }

static void memcpy_le16( uint8_t * dest, const uint16_t val )
{
  uint16_t swizzled = htole16( val );
  memcpy( dest, &swizzled, sizeof( swizzled ) );
}

static void memcpy_le32( uint8_t * dest, const uint32_t val )
{
  uint32_t swizzled = htole32( val );
  memcpy( dest, &swizzled, sizeof( swizzled ) );
}

IVFWriter::IVFWriter( FileDescriptor && fd,
                      const string & fourcc,
                      const uint16_t width,
                      const uint16_t height,
                      const uint32_t frame_rate,
                      const uint32_t time_scale )
  : fd_( move( fd ) ),
    file_size_( 0 ),
    frame_count_( 0 ),
    width_( width ),
    height_( height )
{
  if ( fourcc.size() != 4 ) {
    throw internal_error( "IVF", "FourCC must be four bytes long" );
  }

  /* build the header */
  SafeArray<uint8_t, IVF::supported_header_len> new_header;
  zero( new_header );

  memcpy( &new_header.at( 0 ), "DKIF", 4 );
  /* skip version number (= 0) */
  memcpy_le16( &new_header.at( 6 ), IVF::supported_header_len );
  memcpy( &new_header.at( 8 ), fourcc.data(), 4 );
  memcpy_le16( &new_header.at( 12 ), width );
  memcpy_le16( &new_header.at( 14 ), height );
  memcpy_le32( &new_header.at( 16 ), frame_rate );
  memcpy_le32( &new_header.at( 20 ), time_scale );

  /* write the header */
  fd_.write( Chunk( &new_header.at( 0 ), new_header.size() ) );
  file_size_ += new_header.size();

  /* verify the new file size */
  assert( fd_.size() == file_size_ );
}

void IVFWriter::set_expected_decoder_entry_hash( const uint32_t minihash ) /* ExCamera invention */
{
  /* map the header into memory */
  MMap_Region header_in_mem( IVF::supported_header_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_.fd_num() );
  uint8_t * mutable_header_ptr = header_in_mem.addr();

  memcpy_le32( mutable_header_ptr + 28, minihash ); /* ExCamera invention */
}

size_t IVFWriter::append_frame( const Chunk & chunk )
{
  /* map the header into memory */
  MMap_Region header_in_mem( IVF::supported_header_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_.fd_num() );
  uint8_t * mutable_header_ptr = header_in_mem.addr();

  Chunk header( mutable_header_ptr, IVF::supported_header_len );

  /* verify the existing frame count */
  assert( frame_count_ == header( 24, 4 ).le32() );

  /* verify the existing file size */
  assert( file_size_ == fd_.size() );

  /* build the frame header */
  SafeArray<uint8_t, IVF::frame_header_len> new_header;
  zero( new_header );
  memcpy_le32( &new_header.at( 0 ), chunk.size() );

  /* XXX does not include presentation timestamp */

  /* append the frame header to the file */
  fd_.write( Chunk( &new_header.at( 0 ), new_header.size() ) );
  file_size_ += new_header .size();
  size_t written_offset = file_size_;

  /* append the frame to the file */
  fd_.write( chunk );
  file_size_ += chunk.size();

  /* verify the new file size */
  assert( fd_.size() == file_size_ );

  /* increment the frame count in the file's header */
  frame_count_++;
  memcpy_le32( mutable_header_ptr + 24, frame_count_ );

  /* verify the new frame count */
  assert( frame_count_ == header( 24, 4 ).le32() );

  return written_offset;
}

IVFWriter::IVFWriter( const string & filename,
                      const string & fourcc,
                      const uint16_t width,
                      const uint16_t height,
                      const uint32_t frame_rate,
                      const uint32_t time_scale )
  : IVFWriter( SystemCall( filename,
                           open( filename.c_str(),
                                 O_RDWR | O_CREAT | O_TRUNC,
                                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ),
               fourcc, width, height, frame_rate, time_scale )
{}
