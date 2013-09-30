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

template <class T, int width>
class UnsignedInteger
{
private:
  T i;

public:
  UnsignedInteger( BoolDecoder & data ) : i( data.uint( width ) ) {}
  operator const T & () const { return i; }
};

template <int width>
struct FlagMagSign : public FlaggedType< UnsignedInteger<uint8_t, width> >
{
  Optional<bool> sign;

  FlagMagSign( BoolDecoder & data )
    : FlaggedType< UnsignedInteger<uint8_t, width> >( data ),
      sign( this->initialized() ? data.bit() : Optional<bool>() )
  {}
};

#endif /* VP8_HEADER_STRUCTURES_HH */
