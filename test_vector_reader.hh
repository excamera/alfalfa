/*
 *  Copyright (c) 2013 Anirudh Sivaraman
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_VECTOR_READER_HH_
#define TEST_VECTOR_READER_HH_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>

class TestVectorReader {
 public:
  /* Constructor */
  explicit TestVectorReader(std::string file_name);

  /* Read next frame into buffer */
  int read_frame(uint8_t  **buf,
                 uint32_t *buf_sz,
                 uint32_t *buf_alloc_sz);

  /* Frame hdr sizes for raw and ivf frames*/
  static const uint32_t IVF_FRAME_HDR_SZ = sizeof(uint32_t) + sizeof(uint64_t);
  static const uint32_t RAW_FRAME_HDR_SZ = sizeof(uint32_t);

 private:
  /* Read 2 bytes from buffer */
  unsigned int mem_get_le16(const void *vmem);

  /* Read 4 bytes from buffer */
  unsigned int mem_get_le32(const void *vmem);

  /* Check if infile is an IVF file or not */
  int file_is_ivf(void);

  /* file pointer */
  FILE *infile_;

  /* is it an ivf file */
  unsigned int is_ivf_;
};

#endif  // TEST_VECTOR_READER_HH_
