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

#include "player.hh"
#include "uncompressed_chunk.hh"
#include "decoder_state.hh"

#include <fstream>

using namespace std;

FramePlayer::FramePlayer( const uint16_t width, const uint16_t height )
  : width_( width ),
    height_( height ),
    decoder_( width, height )
{}

FramePlayer::FramePlayer(EncoderStateDeserializer &idata)
  : width_(0)
  , height_(0)
  , decoder_(move(Decoder::deserialize(idata)))
{
  width_ = decoder_.get_width();
  height_ = decoder_.get_height();
}

FramePlayer FramePlayer::deserialize(EncoderStateDeserializer &idata) {
  return FramePlayer(idata);
}

size_t FramePlayer::serialize(EncoderStateSerializer &odata) {
  return decoder_.serialize(odata);
}

Optional<RasterHandle> FramePlayer::decode( const Chunk & chunk )
{
  return decoder_.parse_and_decode_frame( chunk );
}

const VP8Raster & FramePlayer::example_raster( void ) const
{
  return decoder_.example_raster();
}

bool FramePlayer::operator==( const FramePlayer & other ) const
{
  return decoder_ == other.decoder_;
}

bool FramePlayer::operator!=( const FramePlayer & other ) const
{
  return not operator==( other );
}

ostream& operator<<( ostream & out, const FramePlayer & player)
{
  return out << player.decoder_.get_hash().str();
}

FilePlayer::FilePlayer( const string & filename )
  : FilePlayer( filename, IVF( filename ) )
{}

FilePlayer::FilePlayer( const string & filename, IVF && file )
  : FramePlayer( file.width(), file.height() ),
    file_ ( move( file ) ),
    filename_( filename )
{
  if ( file_.fourcc() != "VP80" ) {
    throw Unsupported( "not a VP8 file" );
  }

  // Start at first KeyFrame
  while ( frame_no_ < file_.frame_count() ) {
    UncompressedChunk uncompressed_chunk = decoder_.decompress_frame( file_.frame( frame_no_ ) );
    if ( uncompressed_chunk.key_frame() ) {
      break;
    }
    frame_no_++;
  }
}

FilePlayer::FilePlayer(const string &filename, IVF &&file, EncoderStateDeserializer &idata)
  : FramePlayer(idata)
  , file_(move(file))
  , filename_(filename)
{
  if (file_.fourcc() != "VP80") {
    throw Unsupported( "not a VP8 file" );
  }

  if (file_.width() != decoder_.get_width() || file_.height() != decoder_.get_height()) {
    throw Unsupported("state vs. file dimension mismatch");
  }

  if ( not decoder_.minihash_match( file_.expected_decoder_minihash() ) ) {
    throw Invalid( "Decoder state / IVF mismatch" );
  }
}

FilePlayer FilePlayer::deserialize(EncoderStateDeserializer &idata, const string &filename) {
  return FilePlayer(filename, move(IVF(filename)), idata);
}

size_t FilePlayer::serialize(EncoderStateSerializer &odata) {
  return decoder_.serialize(odata);
}

RasterHandle FilePlayer::advance( void )
{
  while ( not eof() ) {
    Optional<RasterHandle> raster = decode( file_.frame( frame_no_++ ) );
    if ( raster.initialized() ) {
      return raster.get();
    }
  }

  throw Unsupported( "hidden frames at end of file" );
}

bool FilePlayer::eof( void ) const
{
  return frame_no_ == file_.frame_count();
}

long unsigned int FilePlayer::original_size( void ) const
{
  return file_.frame( cur_frame_no() ).size();
}
