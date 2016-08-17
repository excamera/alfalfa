/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// extremely simple serializer and deserializer utility class for packing up encoder states

#pragma once
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
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
  , REF_FLAGS
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

      for (unsigned row = 0; row < height; row++) {
        for (unsigned col = 0; col < width; col++) {
          this->put(ref.Y().at(col, row));
        }
      }

      for (unsigned row = 0; row < height / 2; row++) {
        for (unsigned col = 0; col < width / 2; col++) {
          this->put(ref.U().at(col, row));
        }
      }

      for (unsigned row = 0; row < height / 2; row++) {
        for (unsigned col = 0; col < width / 2; col++) {
          this->put(ref.V().at(col, row));
        }
      }

      return len + 5;
    }

    void write(const char *filename) {
      std::ofstream output_file(filename);
      std::ostream_iterator<uint8_t> output_iter(output_file);
      std::copy(data_.begin(), data_.end(), output_iter);
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

    void reset(void) { ptr_ = 0; }
    size_t remaining(void) const { return size() - ptr_; }

    EncoderSerDesTag get_tag(void) {
      return static_cast<EncoderSerDesTag>((*this)(ptr_++, 1).octet());
    }

    template<typename T> T get(void) {
      T ret = (T) 0;
      for (unsigned i = 0; i < sizeof(ret); i++) {
        ret = ret << 8;
        ret = ret | (*this)(ptr_++, 1).octet();
      }
      return ret;
    }

    MutableRasterHandle get_ref(EncoderSerDesTag t, const uint16_t width, const uint16_t height) {
      MutableRasterHandle raster(width, height);
      EncoderSerDesTag data_type = this->get_tag();
      assert(data_type == t);
      (void) t;           // not used except in assert
      (void) data_type;   // not used except in assert
      unsigned rwidth = raster.get().width();
      unsigned rheight = raster.get().height();

      uint32_t expect_len = rwidth * rheight + 2 * (rwidth / 2) + 2 * (rheight / 2);
      uint32_t get_len = this->get<uint32_t>();
      assert(get_len == expect_len);
      (void) get_len;     // only used in assert
      (void) expect_len;  // only used in assert

      for (unsigned row = 0; row < height; row++) {
        for (unsigned col = 0; col < width; col++) {
          raster.get().Y().at(col, row) = this->get<uint8_t>();
        }
      }

      for (unsigned row = 0; row < height / 2; row++) {
        for (unsigned col = 0; col < width / 2; col++) {
          raster.get().U().at(col, row) = this->get<uint8_t>();
        }
      }

      for (unsigned row = 0; row < height / 2; row++) {
        for (unsigned col = 0; col < width / 2; col++) {
          raster.get().V().at(col, row) = this->get<uint8_t>();
        }
      }
      return raster;
    }
};
