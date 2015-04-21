#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>

#include "ivf.hh"
#include "decoder.hh"

template <class DecoderType = Decoder>
class Player
{
private:
  IVF file_;

  DecoderType decoder_;

  unsigned int frame_no_ { 0 };

public:
  Player( const std::string & file_name );

  RasterHandle advance( void );
  
  bool eof( void ) const;

  unsigned int frame_no( void ) const
  { 
    return frame_no_ - 1;
  }

  long unsigned int original_size( void ) const;

  const Raster & example_raster( void ) const
  {
    return decoder_.example_raster();
  }

  std::vector<uint8_t> operator-( Player & source_player );

  RasterHandle reconstruct_diff( const std::vector<uint8_t> & diff ) const;
};

#endif
