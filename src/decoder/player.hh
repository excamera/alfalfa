#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>
#include <memory>

#include "ivf.hh"
#include "decoder.hh"
#include "serialized_frame.hh"
#include "continuation_state.hh"

class FramePlayer
{
friend std::ostream& operator<<( std::ostream &, const FramePlayer &);
private:
  uint16_t width_, height_;

protected:
  Decoder decoder_; // FIXME ideally this would be private
  Optional<RasterHandle> decode( const Chunk & chunk );

public:
  FramePlayer( const uint16_t width, const uint16_t height );

  Optional<RasterHandle> decode( const SerializedFrame & frame );

  const Raster & example_raster( void ) const;

  bool can_decode( const SerializedFrame & frame ) const;

  uint16_t width( void ) const { return width_; }
  uint16_t height( void ) const { return height_; }

  bool operator==( const FramePlayer & other ) const;
  bool operator!=( const FramePlayer & other ) const;
};

class FilePlayer : public FramePlayer
{
private:
  IVF file_;

  unsigned int frame_no_ { 0 };

  FilePlayer( IVF && file );

protected:
  Chunk get_next_frame( void );

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

using Player = FilePlayer;

#endif
