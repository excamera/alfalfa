#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>
#include <memory>

#include "ivf.hh"
#include "decoder.hh"

template <class DecoderType>
class FramePlayer
{
protected:
  uint16_t width_, height_;

  DecoderType decoder_ { width_, height_ };

public:
  FramePlayer( const uint16_t width, const uint16_t height );

  RasterHandle decode( const std::vector<uint8_t> & raw_frame, const std::string & name );

  const Raster & example_raster( void ) const;

  std::vector<uint8_t> operator-( const FramePlayer & source_player ) const;
  bool operator==( const FramePlayer & other ) const;
  bool operator!=( const FramePlayer & other ) const;
};

template <class DecoderType>
class FilePlayer : public FramePlayer<DecoderType>
{
private:
  IVF file_;

  unsigned int frame_no_ { 0 };

  FilePlayer( IVF & file );

public:
  FilePlayer( const std::string & file_name );

  RasterHandle advance( void );
  
  bool eof( void ) const;

  unsigned int cur_frame_no( void ) const
  { 
    return frame_no_ - 1;
  }

  long unsigned int original_size( void ) const;

};

using Player = FilePlayer<Decoder>;

#endif
