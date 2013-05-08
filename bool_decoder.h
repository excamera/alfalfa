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


static void
init_bool_decoder(struct bool_decoder *d,
                  const unsigned char *start_partition,
                  size_t               sz)
{
    if (sz >= 2)
    {
        d->value = (start_partition[0] << 8) /* first 2 input bytes */
                   | start_partition[1];
        d->input = start_partition + 2;      /* ptr to next byte */
        d->input_len = sz - 2;
    }
    else
    {
        d->value = 0;
        d->input = NULL;
        d->input_len = 0;
    }

    d->range = 255;    /* initial range is full */
    d->bit_count = 0;  /* have not yet shifted out any bits */
}


static int bool_get(struct bool_decoder *d, int probability)
{
    /* range and split are identical to the corresponding values
       used by the encoder when this bool was written */

    unsigned int  split = 1 + (((d->range - 1) * probability) >> 8);
    unsigned int  SPLIT = split << 8;
    int           retval;           /* will be 0 or 1 */

    if (d->value >= SPLIT)    /* encoded a one */
    {
        retval = 1;
        d->range -= split;  /* reduce range */
        d->value -= SPLIT;  /* subtract off left endpoint of interval */
    }
    else                  /* encoded a zero */
    {
        retval = 0;
        d->range = split; /* reduce range, no change in left endpoint */
    }

    while (d->range < 128)    /* shift out irrelevant value bits */
    {
        d->value <<= 1;
        d->range <<= 1;

        if (++d->bit_count == 8)    /* shift in new bits 8 at a time */
        {
            d->bit_count = 0;

            if (d->input_len)
            {
                d->value |= *d->input++;
                d->input_len--;
            }
        }
    }

    return retval;
}


static int bool_get_bit(struct bool_decoder *br)
{
    return bool_get(br, 128);
}


static int bool_get_uint(struct bool_decoder *br, int bits)
{
    int z = 0;
    int bit;

    for (bit = bits - 1; bit >= 0; bit--)
    {
        z |= (bool_get_bit(br) << bit);
    }

    return z;
}


static int bool_get_int(struct bool_decoder *br, int bits)
{
    int z = 0;
    int bit;

    for (bit = bits - 1; bit >= 0; bit--)
    {
        z |= (bool_get_bit(br) << bit);
    }

    return bool_get_bit(br) ? -z : z;
}


static int bool_maybe_get_int(struct bool_decoder *br, int bits)
{
    return bool_get_bit(br) ? bool_get_int(br, bits) : 0;
}


static int
bool_read_tree(struct bool_decoder *bool,
               const int           *t,
               const unsigned char *p)
{
    int i = 0;

    while ((i = t[ i + bool_get(bool, p[i>>1])]) > 0) ;

    return -i;
}
#endif
