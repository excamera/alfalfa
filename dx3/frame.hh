#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock_header.hh"
#include "uncompressed_chunk.hh"

class KeyFrame
{
 private:
  unsigned int macroblock_width, macroblock_height;

  TwoD< Block > Y2_ { macroblock_width, macroblock_height };
  TwoD< Block > Y_ { 4 * macroblock_width, 4 * macroblock_height };
  TwoD< Block > U_ { 2 * macroblock_width, 2 * macroblock_height };
  TwoD< Block > V_ { 2 * macroblock_width, 2 * macroblock_height };

  BoolDecoder first_partition;
  KeyFrameHeader header_ { first_partition };
  KeyFrameHeader::DerivedQuantities probability_tables_ { header_.derived_quantities() };

  Optional< TwoD< KeyFrameMacroblockHeader > > macroblock_headers_ {};

 public:
  KeyFrame( UncompressedChunk & chunk,
	    const unsigned int width,
	    const unsigned int height );
};

#endif /* FRAME_HH */
