#ifndef BLOCK_HH
#define BLOCK_HH

#include "2d.hh"

template <class PredictionMode>
class Block
{
private:
  PredictionMode prediction_mode_ {};
  Optional< Block * > above_;
  Optional< Block * > left_;

public:
  Block( typename TwoD< Block< PredictionMode > >::Context & context )
    : above_( context.above ), left_( context.left )
  {}
};

#endif /* BLOCK_HH */
