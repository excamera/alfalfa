/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef IVF_WRITER_HH
#define IVF_WRITER_HH

#include "ivf.hh"

class IVFWriter
{
private:
  FileDescriptor fd_;
  uint64_t file_size_;
  uint32_t frame_count_;

  uint16_t width_;
  uint16_t height_;

public:
  IVFWriter( const std::string & filename,
             const std::string & fourcc,
             const uint16_t width,
             const uint16_t height,
             const uint32_t frame_rate,
             const uint32_t time_scale );

  IVFWriter( FileDescriptor && fd,
             const std::string & fourcc,
             const uint16_t width,
             const uint16_t height,
             const uint32_t frame_rate,
             const uint32_t time_scale );

  size_t append_frame( const Chunk & chunk );

  void set_expected_decoder_entry_hash( const uint32_t minihash ); /* ExCamera invention */

  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
};

#endif /* IVF_WRITER_HH */
