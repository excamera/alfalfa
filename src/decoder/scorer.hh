/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef SCORER_HH
#define SCORER_HH

#include "safe_array.hh"
#include "vp8_header_structures.hh"

class Scorer
{
private:
  bool motion_vectors_flipped_;

  SafeArray< uint8_t, 4 > scores_ {{}};
  SafeArray< MotionVector, 4 > motion_vectors_ {{}};

  uint8_t splitmv_score_ {};

  uint8_t index_ {};

  void add( const uint8_t score, const MotionVector & mv )
  {
    if ( mv.empty() ) {
      scores_.at( 0 ) += score;
    } else {
      if ( not ( mv == motion_vectors_.at( index_ ) ) ) {
        index_++;
        motion_vectors_.at( index_ ) = mv;
      }

      scores_.at( index_ ) += score;
    }
  }

public:
  Scorer( const bool motion_vectors_flipped ) : motion_vectors_flipped_( motion_vectors_flipped ) {}

  const MotionVector & best( void ) const { return motion_vectors_.at( 0 ); }
  const MotionVector & nearest( void ) const { return motion_vectors_.at( 1 ); }
  const MotionVector & near( void ) const { return motion_vectors_.at( 2 ); }

  void add( const uint8_t score, const Optional< const InterFrameMacroblock * > & mb );

  void calculate( void );

  SafeArray< uint8_t, 4 > mode_contexts( void ) const
  {
    return { { scores_.at( 0 ), scores_.at( 1 ), scores_.at( 2 ), splitmv_score_ } };
  }

  static MotionVector clamp( const MotionVector & mv, const typename TwoD< InterFrameMacroblock >::Context & c );
};

#endif /* SCORER_HH */
