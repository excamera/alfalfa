/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef BIT_OPS_H
#define BIT_OPS_H

/* Evaluates to a mask with n bits set */
#define BITS_MASK(n) ((1<<(n))-1)

/* Returns len bits, with the LSB at position bit */
#define BITS_GET(val, bit, len) (((val)>>(bit))&BITS_MASK(len))

#endif
