#ifndef FRAME_HH
#define FRAME_HH

#include "2d.hh"
#include "block.hh"
#include "macroblock_header.hh"
#include "uncompressed_chunk.hh"
#include "modemv_data.hh"
#include "raster.hh"

class KeyFrame
{
 private:
  unsigned int display_width_, display_height_;
  unsigned int macroblock_width_ { Raster::macroblock_dimension( display_width_ ) },
    macroblock_height_ { Raster::macroblock_dimension( display_height_ ) };

  TwoD< Y2Block > Y2_ { macroblock_width_, macroblock_height_ };
  TwoD< YBlock > Y_ { 4 * macroblock_width_, 4 * macroblock_height_ };
  TwoD< UVBlock > U_ { 2 * macroblock_width_, 2 * macroblock_height_ };
  TwoD< UVBlock > V_ { 2 * macroblock_width_, 2 * macroblock_height_ };

  BoolDecoder first_partition_;
  KeyFrameHeader header_ { first_partition_ };

  std::vector< BoolDecoder > dct_partitions_;

  Optional< KeyFrameHeader::DerivedQuantities > derived_quantities_ {};
  Optional< TwoD< KeyFrameMacroblockHeader > > macroblock_headers_ {};

  void relink_y2_blocks( void );

 public:
  KeyFrame( UncompressedChunk & chunk,
	    const unsigned int width,
	    const unsigned int height );

  void calculate_probability_tables( void );
  void parse_macroblock_headers( void );
  void assign_output_raster( Raster & raster );
  void decode( void );
  void loopfilter( void );
};

#endif /* FRAME_HH */
