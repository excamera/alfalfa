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
  unsigned int width_, height_;
  std::vector< T > storage_;

public:
  struct Context
  {
    const unsigned int column, row;
    const Optional< const T * > left, above_left, above, above_right;

    Context( const unsigned int s_column, const unsigned int s_row, const TwoD & self )
      : column( s_column ), row( s_row ),
	left(        self.maybe_at( column - 1, row ) ),
	above_left(  self.maybe_at( column - 1, row - 1 ) ),
	above(       self.maybe_at( column,     row - 1 ) ),
	above_right( self.maybe_at( column + 1, row - 1 ) )
    {}
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
	const Context c( column, row, *this );
	storage_.emplace_back( c, Fargs... );
      }
    }
  }

  T & at( const unsigned int column, const unsigned int row )
  {
    assert( column < width_ and row < height_ );
    return storage_[ row * width_ + column ];
  }

  const T & at( const unsigned int column, const unsigned int row ) const
  {
    assert( column < width_ and row < height_ );
    return storage_[ row * width_ + column ];
  }

  Optional< const T * > maybe_at( const unsigned int column, const unsigned int row ) const
  {
    if ( column < width_ and row < height_ ) {
      return &at( column, row );
    } else {
      return Optional< const T * >();
    }
  }

  unsigned int width( void ) const { return width_; }
  unsigned int height( void ) const { return height_; }

  template <class lambda>
  void forall( const lambda & f ) const
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ) );
      }
    }
  }

  template <class lambda>
  void forall( const lambda & f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ) );
      }
    }
  }

  template <class lambda>
  void forall_ij( const lambda & f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ), column, row );
      }
    }
  }

  /* forbid copying */
  TwoD( const TwoD & other ) = delete;
  TwoD & operator=( const TwoD & other ) = delete;
};

template< class T, unsigned int sub_width, unsigned int sub_height >
class TwoDSubRange
{
private:
  TwoD< T > & master_;

  unsigned int column_, row_;

public:
  TwoDSubRange( TwoD< T > & master, const unsigned int column, const unsigned int row )
    : master_( master ), column_( column ), row_( row )
  {
    assert( column_ + sub_width <= master_.width() );
    assert( row_ + sub_height <= master_.height() );
  }

  T & at( const unsigned int column, const unsigned int row )
  {
    assert( column < sub_width and row < sub_height );
    return master_.at( column_ + column, row_ + row );
  }

  const T & at( const unsigned int column, const unsigned int row ) const
  {
    assert( column < sub_width and row < sub_height );
    return master_.at( column_ + column, row_ + row );
  }

  unsigned int width( void ) const { return sub_width; }
  unsigned int height( void ) const { return sub_height; }

  TwoDSubRange< T, sub_width, 1 > row( const unsigned int num ) const
  {
    return TwoDSubRange< T, sub_width, 1 >( master_, column_, row_ + num );
  }

  TwoDSubRange< T, 1, sub_height > column( const unsigned int num ) const
  {
    return TwoDSubRange< T, 1, sub_height >( master_, column_ + num, row_ );
  }

  TwoDSubRange< T, sub_width, 1 > bottom( void ) const
  {
    return row( sub_height - 1 );
  }

  TwoDSubRange< T, 1, sub_height > right( void ) const
  {
    return column( sub_width - 1 );
  }

  void set( const TwoDSubRange< T, sub_width, sub_height > & other )
  {
    //    assert( &master_ == &other.master_ );
    column_ = other.column_;
    row_ = other.row_;
  }

  template <class type>
  type sum( const type & initial ) const
  {
    type ret = initial;
    forall( [&] ( const T & x ) { ret += x; } );
    return ret;
  }

  void fill( const T & value )
  {
    forall( [&] ( T & x ) { x = value; } ); 
  }

  template <class lambda>
  void forall( const lambda & f ) const
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ) );
      }
    }
  }

  template <class lambda>
  void forall( const lambda & f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ) );
      }
    }
  }

  template <class lambda>
  void forall_ij( const lambda & f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ), column, row );
      }
    }
  }
};

#endif /* TWOD_HH */
