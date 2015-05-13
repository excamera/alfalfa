#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>
#include <memory>

#include "ivf.hh"
#include "decoder.hh"
#include "serialized_frame.hh"

template <class DecoderType>
class FramePlayer
{
protected:
  uint16_t width_, height_;

  DecoderType decoder_ { width_, height_ };

public:
  FramePlayer( const uint16_t width, const uint16_t height );

  RasterHandle decode( const SerializedFrame & frame );

  const Raster & example_raster( void ) const;

  bool equal_references( const FramePlayer & other ) const;

  void update_continuation( const FramePlayer & other );

  SerializedFrame operator-( const FramePlayer & source_player ) const;
  bool operator==( const FramePlayer & other ) const;
  bool operator!=( const FramePlayer & other ) const;
};

template <class DecoderType>
class FilePlayer : public FramePlayer<DecoderType>
{
private:
  IVF file_;

  unsigned int frame_no_ { 0 };
  unsigned int displayed_frame_no_ { 0 };

  FilePlayer( IVF && file );

public:
  FilePlayer( const std::string & file_name );

  RasterHandle advance( void );

  SerializedFrame serialize_next( void );
  
  bool eof( void ) const;

  unsigned int cur_frame_no( void ) const
  { 
    return frame_no_ - 1;
  }

  unsigned int cur_displayed_frame( void ) const
  {
    return displayed_frame_no_;
  }

  long unsigned int original_size( void ) const;

};

using Player = FilePlayer<Decoder>;

#endif
