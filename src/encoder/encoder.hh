#ifndef ENCODER_HH
#define ENCODER_HH

#include "decoder.hh"
#include "frame.hh"

std::vector< uint8_t > encode_frame( const KeyFrame & frame, const ProbabilityTables & probability_tables );
std::vector< uint8_t > encode_frame( const InterFrame & frame, const ProbabilityTables & probability_tables );

#endif /* DECODER_HH */
