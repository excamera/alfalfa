#include "db.hh"

using namespace std;

SerializableData::SerializableData( const string & filename, const string & magic_number,
  OpenMode mode )
    : filename_( filename ), mode_ ( mode ), good_( false ), magic_number( magic_number )
{
  ifstream fin( filename );

  switch( mode_ ) {
  case OpenMode::READ:
    if ( fin.good() ) {
      good_ = true;
    }
    else {
      good_ = false;
    }

    fin.close();
    break;

  case OpenMode::TRUNCATE:
    fin.close();
    good_ = true;
    break;

  default:
    break;
  }
}
