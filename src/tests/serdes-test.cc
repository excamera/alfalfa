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

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <utility>

#include "decoder.hh"
#include "encoder.hh"
#include "file_descriptor.hh"
#include "raster_handle.hh"

using namespace std;

const unsigned min_width = 1;
const unsigned max_width = 512;
uniform_int_distribution<uint16_t> hwdist(min_width, max_width);

void progress(unsigned i, unsigned num_tests);

EncoderStateDeserializer deser_from_ser(EncoderStateSerializer &&odata);

ProbabilityTables random_probability_tables(default_random_engine &rng);
FilterAdjustments random_filter_adjustments(default_random_engine &rng);

Segmentation random_segmentation(default_random_engine &rng, uint16_t width, uint16_t height);
Segmentation random_segmentation(default_random_engine &rng);

DecoderState random_decoder_state(default_random_engine &rng, uint16_t width, uint16_t height);
DecoderState random_decoder_state(default_random_engine &rng);

References random_references(default_random_engine &rng, uint16_t width, uint16_t height);
References random_references(default_random_engine &rng);
MutableRasterHandle random_mutable_raster_handle(default_random_engine &rng, uint16_t width, uint16_t height);

Decoder random_decoder(default_random_engine &rng);
Encoder random_encoder(default_random_engine &rng);

template<typename T> void run_one_test(T (*gen)(default_random_engine &), default_random_engine &rng, string tname);

int main( int argc, char *argv[] ) {
  unsigned num_tests = 16;
  if (argc > 1) {
    unsigned tnum = (unsigned) abs(atoi(argv[1]));
    if (tnum) { num_tests = tnum; }
  }

  try {
    RasterPoolDebug::allow_resize = true;
    default_random_engine rng;
    { random_device rd; rng.seed(rd()); }

    // ProbabilityTables
    cout << "ProbabilityTables: " << flush;
    for (unsigned i = 0; i < num_tests; i++) {
      progress(i, num_tests);
      run_one_test(random_probability_tables, rng, "ProbabilityTables");
    }

    // Segmentation
    cout << "\nSegmentation:      " << flush;
    for (unsigned i = 0; i < num_tests; i++) {
      progress(i, num_tests);
      run_one_test(random_segmentation, rng, "Segmentation");
    }

    // FilterAdjustments
    cout << "\nFilterAdjustments: " << flush;
    for (unsigned i = 0; i < num_tests; i++) {
      progress(i, num_tests);
      run_one_test(random_filter_adjustments, rng, "FilterAdjustments");
    }

    // DecoderState
    cout << "\nDecoderState:      " << flush;
    for (unsigned i = 0; i < num_tests; i++) {
      progress(i, num_tests);
      run_one_test(random_decoder_state, rng, "DecoderState");
    }

    // References
    cout << "\nReferences:        " << flush;
    for (unsigned i = 0; i < num_tests; i++) {
      progress(i, num_tests);
      run_one_test(random_references, rng, "References");
    }

    // Decoder
    cout << "\nDecoder:           " << flush;
    for (unsigned i = 0; i < num_tests; i++) {
      progress(i, num_tests);
      run_one_test(random_decoder, rng, "Decoder");
    }

    cout << '\n';
  } catch ( const exception & e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

void progress(unsigned i, unsigned num_tests) {
  if (num_tests < 50 || 0 == (i + 1) % (num_tests / 50)) {
    cout << '.' << flush;
  }
}

template<typename T>
void run_one_test(T (*gen)(default_random_engine &), default_random_engine &rng, string tname) {
  EncoderStateSerializer odata = {};

  T _in = (*gen)(rng);
  _in.serialize(odata);

  EncoderStateDeserializer idata = deser_from_ser(move(odata));
  T _out = T::deserialize(idata);

  if (!(_in == _out)) {
    throw runtime_error(tname + "failed: _in and _out do not match");
  }
}

EncoderStateDeserializer deser_from_ser(EncoderStateSerializer &&odata) {
  FILE *f = tmpfile();
  odata.write(f);
  return EncoderStateDeserializer(f);
}

ProbabilityTables random_probability_tables(default_random_engine &rng) {
  ProbabilityTables p;

  for (unsigned i = 0; i < BLOCK_TYPES; i++)
  for (unsigned j = 0; j < COEF_BANDS; j++)
  for (unsigned k = 0; k < PREV_COEF_CONTEXTS; k++)
  for (unsigned l = 0; l < ENTROPY_NODES; l++) {
    p.coeff_probs.at(i).at(j).at(k).at(l) = rng();
  }

  for (unsigned i = 0; i < p.y_mode_probs.size(); i++) {
    p.y_mode_probs.at(i) = rng();
  }

  for (unsigned i = 0; i < p.uv_mode_probs.size(); i++) {
    p.uv_mode_probs.at(i) = rng();
  }

  for (unsigned i = 0; i < 2; i++)
  for (unsigned j = 0; j < MV_PROB_CNT; j++) {
    p.motion_vector_probs.at(i).at(j) = rng();
  }

  return p;
}

Segmentation random_segmentation(default_random_engine &rng) {
  return random_segmentation(rng, hwdist(rng), hwdist(rng));
}

Segmentation random_segmentation(default_random_engine &rng, uint16_t width, uint16_t height) {
  Segmentation s(width, height);

  s.absolute_segment_adjustments = rng();

  for (unsigned i = 0; i < num_segments; i++) {
    s.segment_quantizer_adjustments.at(i) = rng();
    s.segment_filter_adjustments.at(i) = rng();
  }

  s.map.forall([&](uint8_t &f){ f = rng(); });

  return s;
}

FilterAdjustments random_filter_adjustments(default_random_engine &rng) {
  FilterAdjustments f;

  for (unsigned i = 0; i < num_reference_frames; i++) {
    f.loopfilter_ref_adjustments.at(i) = rng();
  }

  for (unsigned i = 0; i < 4; i++) {
    f.loopfilter_mode_adjustments.at(i) = rng();
  }

  return f;
}

DecoderState random_decoder_state(default_random_engine &rng) {
  return random_decoder_state(rng, hwdist(rng), hwdist(rng));
}

DecoderState random_decoder_state(default_random_engine &rng, uint16_t width, uint16_t height) {
  Optional<Segmentation> s;
  Optional<FilterAdjustments> f;

  if (rng()) {
    s = move(random_segmentation(rng, width, height));
  }

  if (rng()) {
    f = move(random_filter_adjustments(rng));
  }

  return DecoderState(width, height, random_probability_tables(rng), move(s), move(f));
}

References random_references(default_random_engine &rng) {
  return random_references(rng, hwdist(rng), hwdist(rng));
}

References random_references(default_random_engine &rng, uint16_t width, uint16_t height) {
  References r(MutableRasterHandle(width, height));

  r.last = random_mutable_raster_handle(rng, width, height);
  r.golden = r.last;
  r.alternative = r.last;

  return r;
}

MutableRasterHandle random_mutable_raster_handle(default_random_engine &rng, uint16_t width, uint16_t height) {
  MutableRasterHandle ras = {width, height};

  ras.get().Y().forall([&](uint8_t &f){ f = rng(); });
  ras.get().U().forall([&](uint8_t &f){ f = rng(); });
  ras.get().V().forall([&](uint8_t &f){ f = rng(); });

  return ras;
}

Decoder random_decoder(default_random_engine &rng) {
  uint16_t width = hwdist(rng);
  uint16_t height = hwdist(rng);
  return Decoder(random_decoder_state(rng, width, height), random_references(rng, width, height));
}
