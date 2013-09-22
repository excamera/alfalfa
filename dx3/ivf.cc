#include <stdexcept>

#include "ivf.hh"
#include "file.hh"

using namespace std;

IVF::IVF( const string & filename )
try :
  file_( filename ),
    header_( file_.block( 0, supported_header_len ) ),
    fourcc_( header_.string( 8, 4 ) ),
    width_( header_.le16( 12 ) ),
    height_( header_.le16( 14 ) ),
    frame_rate_( header_.le32( 16 ) ),
    time_scale_( header_.le32( 20 ) ),
    frame_count_( header_.le32( 24 ) ),
    frame_index_( 0 )
      {
	if ( header_.string( 0, 4 ) != "DKIF" ) {
	  throw Exception( filename, "not an IVF file" );
	}

	if ( header_.le16( 4 ) != 0 ) {
	  throw Exception( filename, "not an IVF version 0 file" );
	}

	if ( header_.le16( 6 ) != supported_header_len ) {
	  throw Exception( filename, "unsupported IVF header length" );
	}

	/* build the index */
	frame_index_.reserve( frame_count_ );

	uint64_t position = supported_header_len;
	for ( uint32_t i = 0; i < frame_count_; i++ ) {
	  Block frame_header = file_.block( position, frame_header_len );
	  const uint32_t frame_len = frame_header.le32( 0 );

	  frame_index_.emplace_back( position + frame_header_len, frame_len );
	  position += frame_header_len + frame_len;
	}
      }
catch ( const out_of_range & e )
  {
    throw Exception( filename, "IVF file truncated" );
  }

Block IVF::frame( const uint32_t & index ) const
{
  auto entry = frame_index_.at( index );
  return file_.block( entry.first, entry.second );
}
