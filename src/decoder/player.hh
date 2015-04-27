#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>
#include <memory>

#include "ivf.hh"
#include "decoder.hh"

template <class DecoderType>
class GenericPlayer
{
private:
  std::shared_ptr<IVF> file_;

  DecoderType decoder_;

  unsigned int frame_no_ { 0 };

public:
  GenericPlayer( const std::string & file_name );

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

  bool operator==( const GenericPlayer & other ) const;

  bool operator!=( const GenericPlayer & other ) const;

  std::vector<uint8_t> operator-( const GenericPlayer & source_player ) const;

  RasterHandle reconstruct_diff( const std::vector<uint8_t> & diff );
};

using Player = GenericPlayer<Decoder>;

#endif
