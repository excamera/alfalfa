#ifndef TWOD_HH
#define TWOD_HH

#include <cassert>
#include <vector>

#include "optional.hh"

/* simple two-dimensional container */
template< class T >
class TwoDBase
{
public:
  virtual unsigned int width( void ) const = 0;
  virtual unsigned int height( void ) const = 0;

  virtual T & at( const unsigned int column, const unsigned int row ) = 0;

  void forall( std::function<void( T & )> && f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ) );
      }
    }
  }

  void forall( std::function<void( T &, const unsigned int, const unsigned int )> && f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ), column, row );
      }
    }
  }
};

template< class T >
class TwoDSubRange;

template< class T >
class TwoD : public TwoDBase< T >
{
private:
  unsigned int width_, height_;
  std::vector< T > storage_;

public:
  struct Context
  {
    const unsigned int column, row;
    const Optional< T * > above, left;
  };

  template< typename... Targs >
  TwoD( const unsigned int width, const unsigned int height, Targs&&... Fargs )
    : width_( width ), height_( height ), storage_()
  {
    assert( width > 0 );
    assert( height > 0 );

    storage_.reserve( width * height );

    /* we want to construct each member separately */
    for ( unsigned int row = 0; row < height; row++ ) {
      for ( unsigned int column = 0; column < width; column++ ) {
	const Optional< T * > above( row > 0    ? &at( column, row - 1 ) : Optional< T * >() );
	const Optional< T * > left ( column > 0 ? &at( column - 1, row ) : Optional< T * >() );
	Context c { column, row, above, left };
	storage_.emplace_back( c, Fargs... );
      }
    }
  }

  T & at( const unsigned int column, const unsigned int row ) override
  {
    return storage_.at( row * width_ + column );
  }

  unsigned int width( void ) const override { return width_; }
  unsigned int height( void ) const override { return height_; }

  TwoDSubRange< T > sub( const unsigned int column,
			 const unsigned int row,
			 const unsigned int width,
			 const unsigned int height )
  {
    return TwoDSubRange< T >( *this, column, row, width, height );
  }

  /* forbid copying */
  TwoD( const TwoD & other ) = delete;
  TwoD & operator=( const TwoD & other ) = delete;
};

template< class T >
class TwoDSubRange : public TwoDBase< T >
{
private:
  TwoD< T > & master_;

  unsigned int column_, row_;
  unsigned int width_, height_;

public:
  TwoDSubRange( TwoD< T > & master,
		const unsigned int column,
		const unsigned int row,
		const unsigned int width,
		const unsigned int height )
    : master_( master ), column_( column ), row_( row ), width_( width ), height_( height )
  {
    assert( column + width <= master.width() );
    assert( row + height <= master.height() );
  }

  T & at( const unsigned int column, const unsigned int row ) override
  {
    return master_.at( column_ + column, row_ + row );
  }

  unsigned int width( void ) const override { return width_; }
  unsigned int height( void ) const override { return height_; }
};

#endif /* TWOD_HH */
