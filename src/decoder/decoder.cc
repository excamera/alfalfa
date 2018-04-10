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

#include "decoder.hh"
#include "uncompressed_chunk.hh"
#include "frame.hh"
#include "decoder_state.hh"

#include <sstream>
#include <boost/functional/hash.hpp>

using namespace std;

Decoder::Decoder( const uint16_t width, const uint16_t height )
  : state_( width, height ),
    references_( width, height )
{}

Decoder::Decoder( DecoderState state, References refs )
  : state_( state ), references_( refs )
{}

Decoder::Decoder(EncoderStateDeserializer &idata)
  : state_(move(DecoderState::deserialize(idata)))
  , references_(move(References::deserialize(idata))) {
  assert(idata.remaining() == 0);
}

size_t Decoder::serialize(EncoderStateSerializer &odata) const {
  odata.reserve(5);
  odata.put(EncoderSerDesTag::DECODER);

  uint32_t len = 0;
  size_t placeholder = odata.put(len);

  // serialize
  len += state_.serialize(odata);
  len += references_.serialize(odata);

  // update length
  odata.put(len, placeholder);

  return len + 5;
}

Decoder Decoder::deserialize(EncoderStateDeserializer &idata) {
  EncoderSerDesTag data_type = idata.get_tag();
  assert(data_type == EncoderSerDesTag::DECODER);
  (void) data_type;   // not used except in assert

  uint32_t len = idata.get<uint32_t>();
  assert(len == idata.remaining());
  (void) len;         // not used except in assert

  return Decoder(idata);
}

UncompressedChunk Decoder::decompress_frame( const Chunk & compressed_frame ) const
{
  /* parse uncompressed data chunk */
  return UncompressedChunk( compressed_frame, state_.width, state_.height, error_concealment_ );
}

template<class FrameType>
FrameType Decoder::parse_frame( const UncompressedChunk & decompressed_frame )
{
  return state_.parse_and_apply<FrameType>( decompressed_frame );
}
template KeyFrame Decoder::parse_frame<KeyFrame>( const UncompressedChunk & decompressed_frame );
template InterFrame Decoder::parse_frame<InterFrame>( const UncompressedChunk & decompressed_frame );

/* Some callers (such as the code that produces SerializedFrames) needs the output Raster
 * regardless of whether or not it is shown, so return a pair with a bool indicating if the
 * output should be shown followed by the actual output. parse_and_decode_frame takes care of
 * turning the pair into an Optional for the normal case */
template<class FrameType>
pair<bool, RasterHandle> Decoder::decode_frame( const FrameType & frame )
{
  /* get a RasterHandle */
  MutableRasterHandle raster { state_.width, state_.height };

  const bool shown = frame.show_frame();

  frame.decode( state_.segmentation, references_, raster );

  frame.loopfilter( state_.segmentation, state_.filter_adjustments, raster );

  RasterHandle immutable_raster( move( raster ) );

  frame.copy_to( immutable_raster, references_ );

  return make_pair( shown, immutable_raster );
}
template pair<bool, RasterHandle> Decoder::decode_frame<KeyFrame>( const KeyFrame & frame );
template pair<bool, RasterHandle> Decoder::decode_frame<InterFrame>( const InterFrame & frame );

/* This function takes care of the full decoding process from decompressing the Chunk
 * to returning a pair with the display status and the decoded raster.
 */
pair<bool, RasterHandle> Decoder::get_frame_output( const Chunk & compressed_frame )
{
  UncompressedChunk decompressed_frame = decompress_frame( compressed_frame );
  if ( decompressed_frame.key_frame() ) {
    return decode_frame( parse_frame<KeyFrame>( decompressed_frame ) );
  } else if ( not decompressed_frame.experimental() ) {
    return decode_frame( parse_frame<InterFrame>( decompressed_frame ) );
  } else {
    throw Unsupported( "experimental" );
  }
}

Optional<RasterHandle> Decoder::parse_and_decode_frame( const Chunk & compressed_frame )
{
  pair<bool, RasterHandle> output = get_frame_output( compressed_frame );
  return make_optional( output.first, output.second );
}

DecoderHash Decoder::get_hash( void ) const
{
  return DecoderHash( state_.hash(), references_.last.hash(),
                      references_.golden.hash(), references_.alternative.hash() );
}

DecoderHash::DecoderHash( size_t state_hash, size_t last_hash, size_t golden_hash, size_t alt_hash )
  : state_hash_( state_hash ),
    last_hash_( last_hash ),
    golden_hash_( golden_hash ),
    alt_hash_( alt_hash )
{}

bool Decoder::operator==( const Decoder & other ) const
{
  return state_ == other.state_ and references_ == other.references_;
}

References::References( const uint16_t width, const uint16_t height )
  : References( MutableRasterHandle( width, height ) )
{}

References::References( MutableRasterHandle && raster )
  : last( move( raster ) ),
    golden( last ),
    alternative( last )
{}

References::References(EncoderStateDeserializer &idata, const uint16_t width, const uint16_t height)
  : last( move( idata.get_ref( EncoderSerDesTag::REF_LAST, width, height ) ) )
  , golden( last )
  , alternative( last )
{}

size_t References::serialize(EncoderStateSerializer &odata) const {
  odata.reserve(9);
  odata.put(EncoderSerDesTag::REFERENCES);

  uint32_t len = 4;
  size_t placeholder = odata.put(len);

  odata.put((uint16_t) last.get().display_width());
  odata.put((uint16_t) last.get().display_height());

  len += odata.put(last, EncoderSerDesTag::REF_LAST);

  // We don't care about golden or alternative reference!
  /*len += odata.put(golden, EncoderSerDesTag::REF_GOLD);

  len += odata.put(alternative, EncoderSerDesTag::REF_ALT);*/

  odata.put(len, placeholder);

  return len + 5;
}

References References::deserialize(EncoderStateDeserializer &idata) {
  EncoderSerDesTag data_type = idata.get_tag();
  assert(data_type == EncoderSerDesTag::REFERENCES);
  (void) data_type;   // unused

  uint32_t get_len = idata.get<uint32_t>();
  assert(get_len <= idata.remaining());
  (void) get_len;     // unused

  uint16_t width = idata.get<uint16_t>();
  uint16_t height = idata.get<uint16_t>();

  return References(idata, width, height);
}

bool References::operator==(const References &other) const {
  return last == other.last and
    golden == other.golden and
    alternative == other.alternative;
}

DecoderState::DecoderState(const unsigned s_width, const unsigned s_height,
                           ProbabilityTables &&p, Optional<Segmentation> &&s,
                           Optional<FilterAdjustments> &&f)
  : width(s_width)
  , height(s_height)
  , probability_tables(move(p))
  , segmentation(move(s))
  , filter_adjustments(move(f))
{}

DecoderState::DecoderState( const unsigned int s_width, const unsigned int s_height )
  : width( s_width ), height( s_height )
{}

DecoderState::DecoderState( const KeyFrameHeader & header,
                            const unsigned int s_width,
                            const unsigned int s_height )
  : width( s_width ), height( s_height ),
    segmentation( header.update_segmentation.initialized(), header, width, height ),
    filter_adjustments( header.mode_lf_adjustments.initialized(), header )
{}

DecoderState::DecoderState(EncoderStateDeserializer &idata, const unsigned s_width, const unsigned s_height)
  : width(s_width)
  , height(s_height)
  , probability_tables(move(ProbabilityTables::deserialize(idata))) {
  EncoderSerDesTag seg_opt = idata.get_tag();
  if (seg_opt == EncoderSerDesTag::OPT_FULL) {
    segmentation = move(Segmentation::deserialize(idata));
  }

  EncoderSerDesTag filt_opt = idata.get_tag();
  if (filt_opt == EncoderSerDesTag::OPT_FULL) {
    filter_adjustments = move(FilterAdjustments::deserialize(idata));
  }
}

bool DecoderState::operator==( const DecoderState & other ) const
{
  return width == other.width
    and height == other.height
    and probability_tables == other.probability_tables
    and segmentation == other.segmentation
    and filter_adjustments == other.filter_adjustments;
}

size_t DecoderState::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_combine( hash_val, width );
  boost::hash_combine( hash_val, height );
  boost::hash_combine( hash_val, probability_tables.hash() );
  if ( segmentation.initialized() ) {
    boost::hash_combine( hash_val, segmentation.get().hash() );
  }
  if ( filter_adjustments.initialized() ) {
    boost::hash_combine( hash_val, filter_adjustments.get().hash() );
  }

  return hash_val;
}

size_t DecoderState::serialize(EncoderStateSerializer &odata) const {
  odata.reserve(9);
  odata.put(EncoderSerDesTag::DECODER_STATE);

  uint32_t len = 4;
  size_t placeholder = odata.put(len);

  odata.put((uint16_t) width);
  odata.put((uint16_t) height);

  len += probability_tables.serialize(odata);

  if (segmentation.initialized()) {
    odata.put(EncoderSerDesTag::OPT_FULL);
    len += segmentation.get().serialize(odata);
  } else {
    odata.put(EncoderSerDesTag::OPT_EMPTY);
  }
  len += 1;

  if (filter_adjustments.initialized()) {
    odata.put(EncoderSerDesTag::OPT_FULL);
    len += filter_adjustments.get().serialize(odata);
  } else {
    odata.put(EncoderSerDesTag::OPT_EMPTY);
  }
  len += 1;

  odata.put(len, placeholder);

  return len + 5;
}

DecoderState DecoderState::deserialize(EncoderStateDeserializer &idata) {
  EncoderSerDesTag data_type = idata.get_tag();
  assert(data_type == EncoderSerDesTag::DECODER_STATE);
  (void) data_type;   // unused

  uint32_t get_len = idata.get<uint32_t>();
  assert(get_len <= idata.remaining());
  (void) get_len;     // unused

  uint16_t width = idata.get<uint16_t>();
  uint16_t height = idata.get<uint16_t>();

  return DecoderState(idata, width, height);
}

size_t FilterAdjustments::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_range( hash_val, loopfilter_ref_adjustments.begin(), loopfilter_ref_adjustments.end() );

  boost::hash_range( hash_val, loopfilter_mode_adjustments.begin(), loopfilter_ref_adjustments.end() );

  return hash_val;
}

size_t FilterAdjustments::serialize(EncoderStateSerializer &odata) const {
  uint32_t len = num_reference_frames + 4;
  odata.reserve(len + 5);
  odata.put(EncoderSerDesTag::FILT_ADJ);
  odata.put(len);

  for (unsigned i = 0; i < num_reference_frames; i++) {
    odata.put(loopfilter_ref_adjustments.at(i));
  }

  for (unsigned i = 0; i < 4; i++) {
    odata.put(loopfilter_mode_adjustments.at(i));
  }

  return len + 5;
}

FilterAdjustments::FilterAdjustments(EncoderStateDeserializer &idata) {
  EncoderSerDesTag data_type = idata.get_tag();
  assert(data_type == EncoderSerDesTag::FILT_ADJ);
  (void) data_type;   // not used except in assert

  uint32_t expect_len = num_reference_frames + 4;
  uint32_t get_len = idata.get<uint32_t>();
  assert(expect_len == get_len);
  (void) expect_len;  // not used except in assert
  (void) get_len;     // "

  for (unsigned i = 0; i < num_reference_frames; i++) {
    loopfilter_ref_adjustments.at(i) = idata.get<int8_t>();
  }

  for (unsigned i = 0; i < 4; i++) {
    loopfilter_mode_adjustments.at(i) = idata.get<int8_t>();
  }
}

size_t Segmentation::hash( void ) const
{
  size_t hash_val = 0;

  boost::hash_combine( hash_val, absolute_segment_adjustments );

  boost::hash_range( hash_val, segment_quantizer_adjustments.begin(),
                     segment_quantizer_adjustments.end() );

  boost::hash_range( hash_val, segment_filter_adjustments.begin(),
                     segment_filter_adjustments.end() );

  boost::hash_range( hash_val, map.begin(), map.end() );

  return hash_val;
}

size_t Segmentation::serialize(EncoderStateSerializer &odata) const {
  uint32_t len = 4 + num_segments + num_segments + map.width() * map.height();
  odata.reserve(len + 5);

  // sneak a bool into the tag
  if (absolute_segment_adjustments) {
    odata.put(EncoderSerDesTag::SEGM_ABS);
  } else {
    odata.put(EncoderSerDesTag::SEGM_REL);
  }
  odata.put(len);

  odata.put((uint16_t) map.width());
  odata.put((uint16_t) map.height());

  for (unsigned i = 0; i < num_segments; i++) {
    odata.put(segment_quantizer_adjustments.at(i));
  }

  for (unsigned i = 0; i < num_segments; i++) {
    odata.put(segment_filter_adjustments.at(i));
  }

  map.forall([&](uint8_t &f){ odata.put(f); });

  return len + 5;
}

Segmentation Segmentation::deserialize(EncoderStateDeserializer &idata) {
  EncoderSerDesTag data_type = idata.get_tag();
  assert(data_type == EncoderSerDesTag::SEGM_ABS || data_type == EncoderSerDesTag::SEGM_REL);
  bool abs = data_type == EncoderSerDesTag::SEGM_ABS;

  uint32_t get_len = idata.get<uint32_t>();
  uint16_t width = idata.get<uint16_t>();
  uint16_t height = idata.get<uint16_t>();
  uint32_t expect_len = 4 + num_segments + num_segments + width * height;
  assert(expect_len == get_len);
  (void) expect_len;    // unused except in assert
  (void) get_len;       // unused except in assert

  return Segmentation(idata, abs, width, height);
}

bool FilterAdjustments::operator==( const FilterAdjustments & other ) const
{
  return loopfilter_ref_adjustments == other.loopfilter_ref_adjustments
    and loopfilter_mode_adjustments == other.loopfilter_mode_adjustments;
}

bool Segmentation::operator==( const Segmentation & other ) const
{
  return absolute_segment_adjustments == other.absolute_segment_adjustments
    and segment_quantizer_adjustments == other.segment_quantizer_adjustments
    and segment_filter_adjustments == other.segment_filter_adjustments
    and map == other.map;
}

Segmentation::Segmentation(const unsigned width, const unsigned height)
  : map(width, height, 3) {}

Segmentation::Segmentation(EncoderStateDeserializer &idata,
                           const bool abs, const unsigned width, const unsigned height)
  : absolute_segment_adjustments(abs)
  , map(width, height, 3) {

  for (unsigned i = 0; i < num_segments; i++) {
    segment_quantizer_adjustments.at(i) = idata.get<int8_t>();
  }

  for (unsigned i = 0; i < num_segments; i++) {
    segment_filter_adjustments.at(i) = idata.get<int8_t>();
  }

  map.forall([&](uint8_t &f){ f = idata.get<uint8_t>(); });
}

Segmentation::Segmentation( const Segmentation & other )
  : absolute_segment_adjustments( other.absolute_segment_adjustments ),
    segment_quantizer_adjustments( other.segment_quantizer_adjustments ),
    segment_filter_adjustments( other.segment_filter_adjustments ),
    map( other.map.width(), other.map.height() )
{
  map.copy_from( other.map );
}

size_t DecoderHash::hash( void ) const
{
  size_t hash_val = 0;
  boost::hash_combine( hash_val, state_hash_ );
  boost::hash_combine( hash_val, last_hash_ );
  boost::hash_combine( hash_val, golden_hash_ );
  boost::hash_combine( hash_val, alt_hash_ );
  return hash_val;
}

string DecoderHash::str( void ) const
{
  stringstream hash_str;
  hash_str << hex;

  hash_str << hash() << " (" << state_hash_ << "_" <<
    last_hash_ << "_" << golden_hash_ << "_" << alt_hash_ << ")";

  return hash_str.str();
}

bool DecoderHash::operator==( const DecoderHash & other ) const
{
  return state_hash_ == other.state_hash_ and
         last_hash_ == other.last_hash_ and
         golden_hash_ == other.golden_hash_ and
         alt_hash_ == other.alt_hash_;
}

bool DecoderHash::operator!=( const DecoderHash & other ) const
{
  return not operator==( other );
}

uint32_t Decoder::minihash() const
{
  return static_cast<uint32_t>( get_hash().hash() );
}


bool Decoder::minihash_match( const uint32_t other_minihash ) const
{
  if ( other_minihash == 0 ) {
    return true;
  }

  return minihash() == other_minihash;
}
