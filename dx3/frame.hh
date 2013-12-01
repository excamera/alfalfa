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

  std::vector< BoolDecoder > dct_partitions_;

  Optional< KeyFrameHeader::DerivedQuantities > probability_tables_ {};
  Optional< TwoD< KeyFrameMacroblockHeader > > macroblock_headers_ {};

  void relink_y2_blocks( void );

 public:
  KeyFrame( UncompressedChunk & chunk,
	    const unsigned int width,
	    const unsigned int height );

  void calculate_probability_tables( void );
  void parse_macroblock_headers( void );
  void parse_tokens( void );
};

#endif /* FRAME_HH */
