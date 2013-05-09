/*
 *  Copyright (c) 2013 Anirudh Sivaraman
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./test_vector_reader.hh"

/* Constructor */
TestVectorReader::TestVectorReader(std::string file_name)
    : infile_(fopen(file_name.c_str(), "rb")),
      is_ivf_(file_is_ivf()) {}

/* Check if infile_ is an IVF file or not */
int TestVectorReader::file_is_ivf(void) {
  char raw_hdr[32];
  int is_ivf = 0;

  if (fread(raw_hdr, 1, 32, infile_) == 32) {
    if (raw_hdr[0] == 'D' && raw_hdr[1] == 'K'
        && raw_hdr[2] == 'I' && raw_hdr[3] == 'F') {
      is_ivf = 1;
      if (mem_get_le16(raw_hdr + 4) != 0)
          fprintf(stderr, "Error: Unrecognized IVF version! This file may not"
                  " decode properly.");
    }
  }

  if (!is_ivf) {
      printf("Exiting. This is not an IVF file \n");
      exit(5);
  }
  return is_ivf;
}

/* Read one frame */
int TestVectorReader::read_frame(uint8_t  **buf,
                              uint32_t *buf_sz,
                              uint32_t *buf_alloc_sz) {
  char     raw_hdr[IVF_FRAME_HDR_SZ];
  uint32_t new_buf_sz;

  /* For both the raw and ivf formats, the frame size is the first 4 bytes
   * of the frame header. We just need to special case on the header
   * size.
   */
  if (fread(raw_hdr, is_ivf_ ? static_cast<uint32_t>(IVF_FRAME_HDR_SZ) : static_cast<uint32_t>(RAW_FRAME_HDR_SZ), 1,
            infile_) != 1) {
    if (!feof(infile_))
        fprintf(stderr, "Failed to read frame size\n");

    new_buf_sz = 0;
  } else {
    new_buf_sz = mem_get_le32(raw_hdr);

    if (new_buf_sz > 256 * 1024 * 1024) {
      fprintf(stderr, "Error: Read invalid frame size (%u)\n",
              new_buf_sz);
      new_buf_sz = 0;
    }

    if (!is_ivf_ && new_buf_sz > 256 * 1024)
        fprintf(stderr, "Warning: Read invalid frame size (%u)"
                " - not a raw file?\n", new_buf_sz);

    if (new_buf_sz > *buf_alloc_sz) {
      uint8_t *new_buf = static_cast<uint8_t*> (realloc(*buf, 2 * new_buf_sz));

      if (new_buf) {
        *buf = new_buf;
        *buf_alloc_sz = 2 * new_buf_sz;
      } else {
        fprintf(stderr, "Failed to allocate compressed data buffer\n");
        new_buf_sz = 0;
      }
    }
  }

  *buf_sz = new_buf_sz;

  if (*buf_sz) {
    if (fread(*buf, 1, *buf_sz, infile_) != *buf_sz) {
        fprintf(stderr, "Failed to read full frame\n");
        return 1;
    }

    return 0;
  }

  return 1;
}

/* Read 2 bytes from buffer */
unsigned int TestVectorReader::mem_get_le16(const void *vmem) {
  unsigned int  val;
  const unsigned char *mem = (const unsigned char *)vmem;

  val = mem[1] << 8;
  val |= mem[0];
  return val;
}

/* Read 4 bytes from buffer */
unsigned int TestVectorReader::mem_get_le32(const void *vmem) {
  unsigned int  val;
  const unsigned char *mem = (const unsigned char *)vmem;

  val = mem[3] << 24;
  val |= mem[2] << 16;
  val |= mem[1] << 8;
  val |= mem[0];
  return val;
}
