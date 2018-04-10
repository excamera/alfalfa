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

#include <algorithm>
#include <iostream>

#include "chunk.hh"
#include "decoder.hh"
#include "enc_state_serializer.hh"
#include "file.hh"

using namespace std;

const unsigned set_bits[256] =
  {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,1,2,2,3,2,3,3,4,2,3,3,4,3
  ,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4
  ,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,1,2,2,3,2,3,3
  ,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5
  ,4,5,5,6,4,5,5,6,5,6,6,7,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,3
  ,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
  };

int main( int argc, char *argv[] )
{
  try {
    if ( argc < 3 ) {
      cout << "Usage: " << argv[0] << " <file1> <file2>" << endl;
      return EXIT_FAILURE;
    }

    string n1(argv[1]);
    string n2(argv[2]);

    // raw bit difference
    {
      File file1(n1);
      File file2(n2);

      // different file sizes: all bits in larger file are "different"
      unsigned diffs = 8 * abs((int) (file1.size() - file2.size()));
      unsigned lim = min<unsigned>(file1.size(), file2.size());
      for (unsigned i = 0; i < lim; i++) {
        uint8_t u1 = file1(i, 1).octet();
        uint8_t u2 = file2(i, 1).octet();

        diffs += set_bits[u1 ^ u2];
      }
      cout << diffs;
      
      if (diffs == 0) {
        cout << '\n';
        return EXIT_SUCCESS;
      } else {
        cout << ' ';
      }
    }

    // count differing pixels in first ref in serialized data
    {
      EncoderStateDeserializer idata1(n1);
      EncoderStateDeserializer idata2(n2);

      // get tags, lengths
      // Decoder tag and len
      idata1.get_tag();
      idata1.get<uint32_t>();

      // Decoder tag and len
      idata2.get_tag();
      idata2.get<uint32_t>();

      // DecoderState tags
      idata1.get_tag();
      idata2.get_tag();

      // DecoderState lens
      uint32_t ds1_len = idata1.get<uint32_t>();
      uint32_t ds2_len = idata2.get<uint32_t>();

      // fast-forward through decoderStates
      for (unsigned i = 0; i < ds1_len; i++) {
        idata1.get<uint8_t>();
      }
      for (unsigned i = 0; i < ds2_len; i++) {
        idata2.get<uint8_t>();
      }

      References r1 = References::deserialize(idata1);
      References r2 = References::deserialize(idata2);

      if (r1.last.get().width() != r2.last.get().width()) {
        throw runtime_error("width mismatch");
      }
      if (r1.last.get().height() != r2.last.get().height()) {
        throw runtime_error("height mismatch");
      }

      unsigned width = r1.last.get().width();
      unsigned height = r1.last.get().height();
      unsigned diff1 = 0;
      unsigned diff2 = 0;
      for (unsigned col = 0; col < r1.last.get().width(); col++) {
        for (unsigned row = 0; row < r1.last.get().height(); row++) {
          if (r1.last.get().Y().at(col, row) != r2.last.get().Y().at(col, row)) diff1++;
          diff2 += abs(r1.last.get().Y().at(col, row) - r2.last.get().Y().at(col, row));
        }
      }

      for (unsigned col = 0; col < r1.last.get().width() / 2; col++) {
        for (unsigned row = 0; row < r1.last.get().height() / 2; row++) {
          if (r1.last.get().U().at(col, row) != r2.last.get().U().at(col, row)) diff1++;
          if (r1.last.get().V().at(col, row) != r2.last.get().V().at(col, row)) diff1++;
          diff2 += abs(r1.last.get().U().at(col, row) - r2.last.get().U().at(col, row));
          diff2 += abs(r1.last.get().V().at(col, row) - r2.last.get().V().at(col, row));
        }
      }
      double tot = width * height + width * height / 2;
      cout << ((double) diff1) / tot << ' '<< ((double) diff2) / ((double) diff1) << endl;
      //cout << diff << endl;
    }
  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_FAILURE;
}
