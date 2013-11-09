#include <array>

#include "mb_records.hh"

using namespace std;

const std::array< uint8_t, num_intra_bmodes - 1 > &
IntraBMode::b_mode_probabilities( const unsigned int ,
				  const Optional< KeyFrameMacroblockHeader * > & ,
				  const Optional< KeyFrameMacroblockHeader * > &  )
{
  return kf_b_mode_probs[ 0 ][ 0 ];
}
