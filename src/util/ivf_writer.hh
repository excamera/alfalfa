#ifndef IVF_WRITER_HH
#define IVF_WRITER_HH

#include "ivf.hh"

class IVFWriter
{
private:
  FileDescriptor fd_;
  uint64_t file_size_;
  uint32_t frame_count_;

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
};

#endif /* IVF_WRITER_HH */
