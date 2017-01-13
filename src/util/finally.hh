#ifndef FINALLY_HH
#define FINALLY_HH

template <typename F>
struct FinalAction
{
  FinalAction( F f )
    : clean_{ f }
  {}

  ~FinalAction()
  {
    if( enabled_ )
      clean_();
  }

  void disable() { enabled_ = false; };

private:
  F clean_;
  bool enabled_{true};
};

template <typename F>
FinalAction<F> finally( F f )
{
  return FinalAction<F>( f );
}

#endif
