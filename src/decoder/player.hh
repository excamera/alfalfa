#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>

#include "ivf.hh"
#include "decoder.hh"

class Player
{
private:
  IVF file_;

  Decoder decoder_;

  unsigned int frame_no_ { 0 };

public:
  Player( const std::string & file_name );

  RasterHandle advance( const bool before_loop_filter = false );
  
  bool eof( void ) const;

  const Raster & example_raster( void ) const
  {
    return decoder_.example_raster();
  }

  std::vector< uint8_t > operator-( Player & source_player );

  RasterHandle reconstruct_diff( const std::vector< uint8_t > & diff );
};

#endif
