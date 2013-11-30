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
  unsigned int macroblock_width_, macroblock_height_;

  TwoD< Y2Block > Y2_ { macroblock_width_, macroblock_height_ };
  TwoD< YBlock > Y_ { 4 * macroblock_width_, 4 * macroblock_height_ };
  TwoD< UBlock > U_ { 2 * macroblock_width_, 2 * macroblock_height_ };
  TwoD< VBlock > V_ { 2 * macroblock_width_, 2 * macroblock_height_ };

  BoolDecoder first_partition_;
  KeyFrameHeader header_ { first_partition_ };
  KeyFrameHeader::DerivedQuantities probability_tables_ { header_.derived_quantities() };

  TwoD< KeyFrameMacroblockHeader > macroblock_headers_ { macroblock_width_, macroblock_height_,
                                                         first_partition_, header_, probability_tables_,
                                                         Y2_, Y_, U_, V_ };

  void relink_y2_blocks( void );

 public:
  KeyFrame( UncompressedChunk & chunk,
	    const unsigned int width,
	    const unsigned int height );
};

#endif /* FRAME_HH */
