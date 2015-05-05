#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>
#include <memory>

#include "ivf.hh"
#include "decoder.hh"

class SerializedFrame
{
private:
  std::vector<uint8_t> frame_;
  std::string source_desc_, target_desc_;

public:
  SerializedFrame( const std::string & path );

  SerializedFrame( const std::vector<uint8_t> & frame, const std::string & source_hash,
		   const std::string & target_hash );

  SerializedFrame( const Chunk & frame, const std::string & source_hash,
		   const std::string & target_hash );

  Chunk chunk( void ) const;

  bool validate_source( const Decoder & decoder ) const;

  bool validate_target( const Decoder & decoder ) const;

  void write( std::string path = "" ) const;

};

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

  std::string hash_str( void ) const;

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

  long unsigned int original_size( void ) const;

};

using Player = FilePlayer<Decoder>;

#endif
