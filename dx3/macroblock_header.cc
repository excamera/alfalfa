#include <vector>

#include "macroblock_header.hh"

using namespace std;

static intra_bmode get_subblock_bmode( const unsigned int position,
				       const intra_mbmode & y_mode,
				       const vector< IntraBMode > & b_modes )
{
  switch ( y_mode ) {
  case DC_PRED: return B_DC_PRED;
  case V_PRED:  return B_VE_PRED;
  case H_PRED:  return B_HE_PRED;
  case TM_PRED: return B_TM_PRED;
  case B_PRED:  return b_modes.at( position );
  }
  assert( false );
  return intra_bmode();  
}

static intra_bmode above_block_mode( const unsigned int position,
				     const Array< IntraBMode, 16 > & prefix,
				     const KeyFrameMacroblockHeader & current,
				     const Optional< KeyFrameMacroblockHeader * > & above )
{
  if ( position < 4 ) {
    return above.initialized()
      ? get_subblock_bmode( position + 12, above.get()->y_mode, above.get()->b_modes.get_or( {} ) )
      : B_DC_PRED;
  } else {
    return get_subblock_bmode( position - 4, current.y_mode, prefix );
  }
}

static intra_bmode left_block_mode( const unsigned int position,
				    const Array< IntraBMode, 16 > & prefix,
				    const KeyFrameMacroblockHeader & current,
				    const Optional< KeyFrameMacroblockHeader * > & left )
{
  if ( not (position & 3) ) {
    return left.initialized()
      ? get_subblock_bmode( position + 3, left.get()->y_mode, left.get()->b_modes.get_or( {} ) )
      : B_DC_PRED;
  } else {
    return get_subblock_bmode( position - 1, current.y_mode, prefix );
  }
}

const std::array< uint8_t, num_intra_bmodes - 1 > &
IntraBMode::b_mode_probabilities( const unsigned int position,
				  const Array< IntraBMode, 16 > & prefix,
				  const KeyFrameMacroblockHeader & current,
				  const Optional< KeyFrameMacroblockHeader * > & above,
				  const Optional< KeyFrameMacroblockHeader * > & left )
{
  return kf_b_mode_probs.at( above_block_mode( position, prefix, current, above ) ).at( left_block_mode( position, prefix, current, left ) );
}
