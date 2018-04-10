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

#include <stdexcept>

#include "ivf.hh"
#include "file.hh"

using namespace std;

IVF::IVF( const string & filename )
try :
  file_( filename ),
    header_( file_( 0, supported_header_len ) ),
    fourcc_( header_( 8, 4 ).to_string() ),
    width_( header_( 12, 2 ).le16() ),
    height_( header_( 14, 2 ).le16() ),
    frame_rate_( header_( 16, 4 ).le32() ),
    time_scale_( header_( 20, 4 ).le32() ),
    frame_count_( header_( 24, 4 ).le32() ),
    expected_decoder_minihash_( header_( 28, 4 ).le32() ),
    frame_index_()
      {
        if ( header_( 0, 4 ).to_string() != "DKIF" ) {
          throw Invalid( "missing IVF file header" );
        }

        if ( header_( 4, 2 ).le16() != 0 ) {
          throw Unsupported( "not an IVF version 0 file" );
        }

        if ( header_( 6, 2 ).le16() != supported_header_len ) {
          throw Unsupported( "unsupported IVF header length" );
        }

        /* build the index */
        frame_index_.reserve( frame_count_ );

        uint64_t position = supported_header_len;
        for ( uint32_t i = 0; i < frame_count_; i++ ) {
          Chunk frame_header = file_( position, frame_header_len );
          const uint32_t frame_len = frame_header.le32();

          frame_index_.emplace_back( position + frame_header_len, frame_len );
          position += frame_header_len + frame_len;
        }
      }
catch ( const out_of_range & e )
  {
    throw Invalid( "IVF file truncated" );
  }

Chunk IVF::frame( const uint32_t & index ) const
{
  const auto & entry = frame_index_.at( index );
  return file_( entry.first, entry.second );
}
