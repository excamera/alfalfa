#include "block.hh"

Block::Block( TwoD< Block >::Context & context )
  : above_( context.above ),
    left_( context.left )
{}
