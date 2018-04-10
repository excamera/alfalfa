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

// extremely simple serializer and deserializer utility class for packing up encoder states

#pragma once
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "file.hh"
#include "file_descriptor.hh"

enum class EncoderSerDesTag : uint8_t
  { PROB_TABLE
  , FILT_ADJ
  , SEGM_ABS
  , SEGM_REL
  , DECODER_STATE
  , OPT_EMPTY
  , OPT_FULL
  , REFERENCES
  , REF_LAST
  , REF_GOLD
  , REF_ALT
  , DECODER
  };

class EncoderStateSerializer {
  private:
    std::vector<uint8_t> data_ = {};

  public:
    EncoderStateSerializer() {}

    void reserve(size_t n) {
      data_.reserve(data_.size() + n);
    }

    size_t put(EncoderSerDesTag t) {
      size_t posn = data_.size();
      data_.push_back((uint8_t) t);
      return posn;
    }

    template <typename T> size_t put(T u) {
      size_t posn = data_.size();
      for (unsigned i = 0; i < sizeof(u); i++) {
        data_.push_back((uint8_t) (u & 0xff));
        u = u >> 8;
      }
      return posn;
    }

    template<typename T> size_t put(T u, size_t offset) {
      for (unsigned i = 0; i < sizeof(u); i++) {
        data_.at(offset + i) = (uint8_t) (u & 0xff);
        u = u >> 8;
      }
      return offset;
    }

    size_t put(const VP8Raster &ref, EncoderSerDesTag t) {
      unsigned width = ref.width();
      unsigned height = ref.height();
      uint32_t len = width * height + 2 * (width / 2) * (height / 2);
      this->reserve(5 + len);
      this->put(t);
      this->put(len);

      ref.Y().forall([this](uint8_t &f){ this->put(f); });
      ref.U().forall([this](uint8_t &f){ this->put(f); });
      ref.V().forall([this](uint8_t &f){ this->put(f); });

      return len + 5;
    }

    void write(const char *filename) {
      std::ofstream output_file(filename);
      std::ostream_iterator<uint8_t> output_iter(output_file);
      std::copy(data_.begin(), data_.end(), output_iter);
    }

    void write(const std::string &filename) {
      this->write(filename.c_str());
    }

    void write(FILE *file) {
      std::fwrite(data_.data(), 1, data_.size(), file);
    }
};

class EncoderStateDeserializer : File {
  private:
    size_t ptr_;

  public:
    EncoderStateDeserializer(const char *filename)
      : File(filename)
      , ptr_(0) {}

    EncoderStateDeserializer(const std::string &filename)
      : File(filename.c_str())
      , ptr_(0) {}

    EncoderStateDeserializer(FILE *file)
      : File(std::move(FileDescriptor(file)))
      , ptr_(0) {}

    template<typename T, typename F, typename ...Ps> static T build(F f, Ps ...ps) {
      EncoderStateDeserializer idata(f);
      return T::deserialize(idata, std::forward<Ps>(ps)...);
    }

    void reset(void) { ptr_ = 0; }
    size_t remaining(void) const { return chunk().size() - ptr_; }
    size_t size(void) const { return chunk().size(); }

    EncoderSerDesTag peek_tag(void) {
      return static_cast<EncoderSerDesTag>((*this)(ptr_, 1).octet());
    }

    EncoderSerDesTag get_tag(void) {
      return static_cast<EncoderSerDesTag>((*this)(ptr_++, 1).octet());
    }

    template<typename T> T get(void) {
      T ret = (T) 0;
      for (unsigned i = sizeof(ret); i > 0; i--) {
        ret = ret << 8;
        ret = ret | (*this)(ptr_ + i - 1, 1).octet();
      }
      ptr_ += sizeof(ret);
      return ret;
    }

    MutableRasterHandle get_ref(EncoderSerDesTag t, const uint16_t width, const uint16_t height) {
      MutableRasterHandle raster(width, height);

      if (this->peek_tag() == t) {
        EncoderSerDesTag data_type = this->get_tag();
        assert(data_type == t);
        (void) t;           // not used except in assert
        (void) data_type;   // not used except in assert
        unsigned rwidth = raster.get().width();
        unsigned rheight = raster.get().height();

        uint32_t expect_len = rwidth * rheight + 2 * (rwidth / 2) * (rheight / 2);
        uint32_t get_len = this->get<uint32_t>();
        assert(get_len == expect_len);
        (void) get_len;     // only used in assert
        (void) expect_len;  // only used in assert

        raster.get().Y().forall([this](uint8_t &f){ f = this->get<uint8_t>(); });
        raster.get().U().forall([this](uint8_t &f){ f = this->get<uint8_t>(); });
        raster.get().V().forall([this](uint8_t &f){ f = this->get<uint8_t>(); });
      }

      return raster;
    }
};
