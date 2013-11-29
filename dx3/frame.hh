#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock_header.hh"
#include "uncompressed_chunk.hh"
#include "modemv_data.hh"

class KeyFrame
{
 private:
  unsigned int macroblock_width, macroblock_height;

  TwoD< Block< intra_mbmode > > Y2_ { macroblock_width, macroblock_height };
  TwoD< Block< intra_bmode > > Y_ { 4 * macroblock_width, 4 * macroblock_height };
  TwoD< Block< intra_mbmode > > U_ { 2 * macroblock_width, 2 * macroblock_height };
  TwoD< Block< intra_mbmode > > V_ { 2 * macroblock_width, 2 * macroblock_height };

  BoolDecoder first_partition;
  KeyFrameHeader header_ { first_partition };
  KeyFrameHeader::DerivedQuantities probability_tables_ { header_.derived_quantities() };

  TwoD< KeyFrameMacroblockHeader > macroblock_headers_ { macroblock_width, macroblock_height,
                                                         first_partition, header_, probability_tables_,
                                                         Y2_, Y_, U_, V_ };

 public:
  KeyFrame( UncompressedChunk & chunk,
	    const unsigned int width,
	    const unsigned int height );
};

#endif /* FRAME_HH */
