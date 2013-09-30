#ifndef VP8_HEADER_STRUCTURES_HH
#define VP8_HEADER_STRUCTURES_HH

#include "optional.hh"

template <class T>
class FlaggedType : public Optional<T>
{
public:
  FlaggedType( BoolDecoder & data )
    : Optional<T>( data.bit() ? T( data ) : Optional<T>() )
  {}
};

template <int width>
class Unsigned
{
private:
  uint8_t i_;

public:
  Unsigned( BoolDecoder & data ) : i_( data.uint( width ) )
  {
    static_assert( width <= 8, "Unsigned width must be <= 8" );
  }
  operator const uint8_t & () const { return i_; }
};

class Bool
{
private:
  bool i_;

public:
  Bool( BoolDecoder & data ) : i_( data.bit() ) {}
  operator const bool & () const { return i_; }
};

template <int width>
struct FlagMagSign : public FlaggedType< Unsigned<width> >
{
  Optional<bool> sign;

  FlagMagSign( BoolDecoder & data )
    : FlaggedType< Unsigned<width> >( data ),
      sign( this->initialized() ? data.bit() : Optional<bool>() )
  {}
};

#endif /* VP8_HEADER_STRUCTURES_HH */
