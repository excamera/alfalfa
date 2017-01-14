/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>
#include <memory>

#include "ivf.hh"
#include "decoder.hh"
#include "enc_state_serializer.hh"

class FramePlayer
{
friend std::ostream& operator<<( std::ostream &, const FramePlayer &);
private:
  uint16_t width_, height_;

protected:
  Decoder decoder_; // FIXME ideally this would be private

public:
  FramePlayer( const uint16_t width, const uint16_t height );
  FramePlayer( EncoderStateDeserializer &idata );

  Optional<RasterHandle> decode( const Chunk & chunk );

  const VP8Raster & example_raster( void ) const;

  uint16_t width( void ) const { return width_; }
  uint16_t height( void ) const { return height_; }

  bool operator==( const FramePlayer & other ) const;
  bool operator!=( const FramePlayer & other ) const;

  const Decoder & current_decoder() const { return decoder_; }
  References current_references() const { return decoder_.get_references(); }
  DecoderState current_state() const { return decoder_.get_state(); }

  void set_decoder( Decoder & decoder ) { decoder_ = decoder; }

  size_t serialize(EncoderStateSerializer &odata);
  static FramePlayer deserialize(EncoderStateDeserializer &idata);

  void set_error_concealment( const bool value ) { decoder_.set_error_concealment( value ); }
};

class FilePlayer : public FramePlayer
{
private:
  IVF file_;
  unsigned int frame_no_ { 0 };
  std::string filename_;
  FilePlayer( const std::string & filename, IVF && file );
  FilePlayer( const std::string & filename, IVF && file, EncoderStateDeserializer & idata );

public:
  FilePlayer( const std::string & filename );

  RasterHandle advance();
  bool eof() const;
  unsigned int cur_frame_no() const { return frame_no_ - 1; }

  long unsigned int original_size() const;

  size_t serialize(EncoderStateSerializer &odata);
  static FilePlayer deserialize(EncoderStateDeserializer &idata, const std::string &filename);
};

using Player = FilePlayer;

#endif
