/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "yuv4mpeg.hh"

#include <fcntl.h>
#include <sstream>
#include <utility>

using namespace std;

YUV4MPEGHeader::YUV4MPEGHeader()
  : width( 0 ), height( 0 ), fps_numerator( 0 ), fps_denominator( 0 ),
    pixel_aspect_ratio_numerator( 0 ), pixel_aspect_ratio_denominator( 0 ),
    interlacing_mode( PROGRESSIVE ), color_space( C420 )
{}

size_t YUV4MPEGHeader::frame_length()
{
  switch ( color_space ) {
    case C420:
    case C420jpeg:
    case C420paldv:
      return ( width * height * 3 ) / 2;
    default: throw LogicError();
  }
}

size_t YUV4MPEGHeader::y_plane_length()
{
  switch ( color_space ) {
    case C420:
    case C420jpeg:
    case C420paldv:
      return ( width * height );
    default: throw LogicError();
  }
}

size_t YUV4MPEGHeader::uv_plane_length()
{
  switch ( color_space ) {
    case C420:
    case C420jpeg:
    case C420paldv:
      return ( width * height / 4 );
    default: throw LogicError();
  }
}

pair< size_t, size_t > YUV4MPEGReader::parse_fraction( const string & fraction_str )
{
  size_t colon_pos = fraction_str.find( ":" );

  if ( colon_pos == string::npos ) {
    throw runtime_error( "invalid fraction" );
  }

  size_t numerator = 0;
  size_t denominator = 0;

  numerator = stoi( fraction_str.substr( 0, colon_pos ) );
  denominator = stoi( fraction_str.substr( colon_pos + 1 ) );

  return make_pair( numerator, denominator );
}

YUV4MPEGReader::YUV4MPEGReader( const string & filename )
  : YUV4MPEGReader( SystemCall( filename,
                    open( filename.c_str(),
                    O_RDWR | O_CREAT,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ) )
{}

YUV4MPEGReader::YUV4MPEGReader( FileDescriptor && fd )
  : header_(),
    fd_( move( fd ) )
{
  string header_str = fd_.getline();
  istringstream ssin( header_str );

  string token;
  ssin >> token;

  if ( token != "YUV4MPEG2" ) {
    throw runtime_error( "invalid yuv4mpeg2 magic code" );
  }

  while ( ssin >> token ) {
    if ( token.length() == 0 ) {
      break;
    }

    switch ( token[0] ) {
    case 'W': // width
      header_.width = stoi( token.substr( 1 ) );
      break;

    case 'H': // height
      header_.height = stoi( token.substr( 1 ) );
      break;

    case 'F': // framerate
    {
      pair< size_t, size_t > fps = YUV4MPEGReader::parse_fraction( token.substr( 1 ) );
      header_.fps_numerator = fps.first;
      header_.fps_denominator = fps.second;
      break;
    }

    case 'I':
      if ( token.length() < 2 ) {
        throw runtime_error( "invalid interlacing mode" );
      }

      switch ( token[ 1 ] ) {
      case 'p': header_.interlacing_mode = YUV4MPEGHeader::InterlacingMode::PROGRESSIVE; break;
      case 't': header_.interlacing_mode = YUV4MPEGHeader::InterlacingMode::TOP_FIELD_FIRST; break;
      case 'b': header_.interlacing_mode = YUV4MPEGHeader::InterlacingMode::BOTTOM_FIELD_FIRST; break;
      case 'm': header_.interlacing_mode = YUV4MPEGHeader::InterlacingMode::MIXED_MODES; break;
      default: throw runtime_error( "invalid interlacing mode" );
      }
      break;

    case 'A': // pixel aspect ratio
    {
      pair< size_t, size_t > aspect_ratio = YUV4MPEGReader::parse_fraction( token.substr( 1 ) );
      header_.pixel_aspect_ratio_numerator = aspect_ratio.first;
      header_.pixel_aspect_ratio_denominator = aspect_ratio.second;
      break;
    }

    case 'C': // color space
        if ( token.substr( 0, 4 ) != "C420" ) {
          throw runtime_error( "only yuv420 color space is supported" );
        }
        header_.color_space = YUV4MPEGHeader::ColorSpace::C420;
        break;

    case 'X': // comment
      break;

    default:
      throw runtime_error( "invalid yuv4mpeg2 input format" );
    }
  }

  if ( header_.width == 0 or header_.height == 0 ) {
    throw runtime_error( "width or height missing" );
  }
}

Optional<RasterHandle> YUV4MPEGReader::get_next_frame()
{
  MutableRasterHandle raster { header_.width, header_.height };

  string frame_header = fd_.getline();

  if ( fd_.eof() ) {
    return Optional<RasterHandle>();
  }

  if ( frame_header != "FRAME" ) {
    throw runtime_error( "invalid yuv4mpeg2 input format" );
  }

  const size_t CHUNK_SIZE = 500 * 1024;
  string read_data;

  for ( size_t byte = 0; byte < y_plane_length(); byte += read_data.length() ) {
    read_data = fd_.read( min( CHUNK_SIZE, y_plane_length() - byte ) );

    for ( size_t i = 0; i < read_data.length(); i++ ) {
      raster.get().Y().at( ( i + byte ) % header_.width,
                           ( i + byte ) / header_.width ) = read_data[ i ];
    }
  }

  for ( size_t byte = 0; byte < uv_plane_length(); byte += read_data.length() ) {
    read_data = fd_.read( min( CHUNK_SIZE, uv_plane_length() - byte ) );

    for ( size_t i = 0; i < read_data.length(); i++ ) {
      raster.get().U().at( ( i + byte ) % ( header_.width / 2 ),
                           ( i + byte ) / ( header_.width / 2 ) ) = read_data[ i ];
    }
  }

  for ( size_t byte = 0; byte < uv_plane_length(); byte += read_data.length() ) {
    read_data = fd_.read( min( CHUNK_SIZE, uv_plane_length() - byte ) );

    for ( size_t i = 0; i < read_data.length(); i++ ) {
      raster.get().V().at( ( i + byte ) % ( header_.width / 2 ),
                           ( i + byte ) / ( header_.width / 2 ) ) = read_data[ i ];
    }
  }

  RasterHandle handle( move( raster ) );
  return make_optional<RasterHandle>( true, handle );
}
