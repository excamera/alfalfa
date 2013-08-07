/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license and
 *  patent grant that can be found in the LICENSE file in the root of
 *  the source tree. All contributing project authors may be found in
 *  the AUTHORS file in the root of the source tree.
 */


#ifndef BOOL_DECODER_H
#define BOOL_DECODER_H
#include <stddef.h>

struct bool_decoder
{
    const unsigned char *input;      /* next compressed data byte */
    size_t               input_len;  /* length of the input buffer */
    unsigned int         range;      /* identical to encoder's range */
    unsigned int         value;      /* contains at least 8 significant
                                      * bits */
    int                  bit_count;  /* # of bits shifted out of value,
                                      * max 7 */
};


void init_bool_decoder(struct bool_decoder *d,
                       const unsigned char *start_partition,
                       size_t               sz);

int bool_get(struct bool_decoder *d, int probability);

int bool_get_bit(struct bool_decoder *br);

int bool_get_uint(struct bool_decoder *br, int bits);

int bool_get_int(struct bool_decoder *br, int bits);

int bool_maybe_get_int(struct bool_decoder *br, int bits);

int bool_read_tree(struct bool_decoder *br,
                   const int           *t,
                   const unsigned char *p);
#endif
