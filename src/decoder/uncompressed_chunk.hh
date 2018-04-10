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

#ifndef UNCOMPRESSED_CHUNK_HH
#define UNCOMPRESSED_CHUNK_HH

#include "chunk.hh"
#include "loopfilter.hh"

#include <vector>

enum CorruptionLevel
{
  NO_CORRUPTION,
  CORRUPTED_RESIDUES,
  CORRUPTED_FIRST_PARTITION,
  CORRUPTED_FRAME
};

class UncompressedChunk
{
private:
  enum class ReconstructionFilterType : char {
    Bicubic,
    Bilinear,
    NoFilter
  };

  bool key_frame_;
  ReconstructionFilterType reconstruction_filter_;
  LoopFilterType loop_filter_;
  bool show_frame_;
  bool experimental_;
  Chunk first_partition_;
  Chunk rest_;
  CorruptionLevel corruption_level_;

public:
  UncompressedChunk( const Chunk & frame, const uint16_t expected_width,
                     const uint16_t expected_height, const bool accept_partial );
  bool key_frame( void ) const { return key_frame_; }

  const Chunk & first_partition( void ) const { return first_partition_; }
  const std::vector< Chunk > dct_partitions( const uint8_t num ) const;

  LoopFilterType loop_filter_type( void ) const { return loop_filter_; }
  bool show_frame( void ) const { return show_frame_; }
  bool experimental( void ) const { return experimental_; }

  CorruptionLevel corruption_level() const { return corruption_level_; }
};

#endif /* UNCOMPRESSED_CHUNK_HH */
