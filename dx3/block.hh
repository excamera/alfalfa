#ifndef BLOCK_HH
#define BLOCK_HH

#include "2d.hh"
#include "block.hh"

class Block
{
private:
  Optional< Block * > above_ {};
  Optional< Block * > left_ {};

public:
  Block( TwoD< Block >::Context &  ) {};
};

#endif /* BLOCK_HH */
