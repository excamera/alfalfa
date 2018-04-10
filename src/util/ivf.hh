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

#ifndef IVF_HH
#define IVF_HH

#include <string>
#include <cstdint>
#include <vector>

#include "file.hh"

class IVF
{
private:
  File file_;
  Chunk header_;

  std::string fourcc_;
  uint16_t width_, height_;
  uint32_t frame_rate_, time_scale_, frame_count_;
  uint32_t expected_decoder_minihash_;

  std::vector< std::pair<uint64_t, uint32_t> > frame_index_;

public:
  static constexpr int supported_header_len = 32;
  static constexpr int frame_header_len = 12;

  IVF( const std::string & filename );

  const std::string & fourcc( void ) const { return fourcc_; }
  uint16_t width( void ) const { return width_; }
  uint16_t height( void ) const { return height_; }
  uint32_t frame_rate( void ) const { return frame_rate_; }
  uint32_t time_scale( void ) const { return time_scale_; }
  uint32_t frame_count( void ) const { return frame_count_; }

  Chunk frame( const uint32_t & index ) const;

  size_t size() const { return file_.size(); }

  uint32_t expected_decoder_minihash() const { return expected_decoder_minihash_; }
};

#endif /* IVF_HH */
