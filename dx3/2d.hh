#ifndef TWOD_HH
#define TWOD_HH

#include <cassert>
#include <vector>

#include "optional.hh"

/* simple two-dimensional container */
template< class T >
class TwoD
{
private:
  std::vector< std::vector< T > > storage_;

public:
  struct Context
  {
    const unsigned int column, row;
    const Optional< T * > above, left;
  };

  template< typename... Targs >
  TwoD( const unsigned int width, const unsigned int height, Targs&&... Fargs )
    : storage_()
  {
    assert( width > 0 );
    assert( height > 0 );

    /* we need to construct each member separately */
    storage_.reserve( height );
    for ( unsigned int i = 0; i < height; i++ ) {
      std::vector< T > row;
      row.reserve( width );
      for ( unsigned int j = 0; j < width; j++ ) {
	const Optional< T * > above( i > 0 ? &storage_.at( i - 1 ).at( j ) : Optional< T * >() );
	const Optional< T * > left ( j > 0 ? &row.at( j - 1 ) : Optional< T * >() );
	Context c { j, i, above, left };
	row.emplace_back( c, Fargs... );
      }
      storage_.emplace_back( move( row ) );
    }
  }

  T & at( const unsigned int column, const unsigned int row )
  {
    return storage_.at( row ).at( column );
  }

  unsigned int width( void ) const { return storage_.at( 0 ).size(); }
  unsigned int height( void ) const { return storage_.size(); }  
};

template< class T >
class TwoDSubRange
{
private:
  TwoD< T > & master_;

  unsigned int row_, column_;
  unsigned int width_, height_;

public:
  TwoDSubRange( TwoD< T > & master,
		const unsigned int column,
		const unsigned int row,
		const unsigned int width,
		const unsigned int height )
    : master_( master ), row_( row ), column_( column ), width_( width ), height_( height )
  {}

  T & at( const unsigned int column, const unsigned int row )
  {
    return master_.at( column_ + column, row_ + row );
  }

  unsigned int width( void ) const { return width_; }
  unsigned int height( void ) const { return height_; }

  void forall( std::function<void( T & )> && f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ) );
      }
    }
  }
	       
};

#endif /* TWOD_HH */
