#include "token_costs.hh"

/*
 * This file is based on
 * [1] libvpx:vp8/encoder/rdopt.c
 * [2] libvpx:vp8/encoder/entropy.c
 * [3] libvpx:vp8/encoder/boolhuff.c:23
 */

const uint16_t vp8_prob_cost[ 256 ] =
{
 2047, 2047, 1791, 1641, 1535, 1452, 1385, 1328, 1279, 1235, 1196, 1161, 1129, 1099, 1072, 1046,
 1023, 1000, 979,  959,  940,  922,  905,  889,  873,  858,  843,  829,  816,  803,  790,  778,
 767,  755,  744,  733,  723,  713,  703,  693,  684,  675,  666,  657,  649,  641,  633,  625,
 617,  609,  602,  594,  587,  580,  573,  567,  560,  553,  547,  541,  534,  528,  522,  516,
 511,  505,  499,  494,  488,  483,  477,  472,  467,  462,  457,  452,  447,  442,  437,  433,
 428,  424,  419,  415,  410,  406,  401,  397,  393,  389,  385,  381,  377,  373,  369,  365,
 361,  357,  353,  349,  346,  342,  338,  335,  331,  328,  324,  321,  317,  314,  311,  307,
 304,  301,  297,  294,  291,  288,  285,  281,  278,  275,  272,  269,  266,  263,  260,  257,
 255,  252,  249,  246,  243,  240,  238,  235,  232,  229,  227,  224,  221,  219,  216,  214,
 211,  208,  206,  203,  201,  198,  196,  194,  191,  189,  186,  184,  181,  179,  177,  174,
 172,  170,  168,  165,  163,  161,  159,  156,  154,  152,  150,  148,  145,  143,  141,  139,
 137,  135,  133,  131,  129,  127,  125,  123,  121,  119,  117,  115,  113,  111,  109,  107,
 105,  103,  101,  99,   97,   95,   93,   92,   90,   88,   86,   84,   82,   81,   79,   77,
 75,   73,   72,   70,   68,   66,   65,   63,   61,   60,   58,   56,   55,   53,   51,   50,
 48,   46,   45,   43,   41,   40,   38,   37,   35,   33,   32,   30,   29,   27,   25,   24,
 22,   21,   19,   18,   16,   15,   13,   12,   10,   9,    7,    6,    4,    3,    1,    1
};

uint8_t  inline TokenCosts::complement( uint8_t prob ) { return 255 - prob; }
uint16_t inline TokenCosts::cost_zero( uint8_t prob )  { return vp8_prob_cost[ prob ]; }
uint16_t inline TokenCosts::cost_one( uint8_t prob )   { return vp8_prob_cost[ complement( prob ) ]; }
uint16_t inline TokenCosts::cost_bit( uint8_t prob, bool b ) { return cost_zero( b ? complement( prob ): prob ); }

/* Although we didn't use this tree in other parts of the code, token cost
 * computation code can be written much cleaner with the help of this.
 */
const int8_t vp8_coef_tree[ 22 ] =     /* corresponding _CONTEXT_NODEs */
{
  -DCT_EOB_TOKEN, 2,                       /* 0 = EOB */
  -ZERO_TOKEN, 4,                          /* 1 = ZERO */
  -ONE_TOKEN, 6,                           /* 2 = ONE */
  8, 12,                                   /* 3 = LOW_VAL */
  -TWO_TOKEN, 10,                          /* 4 = TWO */
  -THREE_TOKEN, -FOUR_TOKEN,               /* 5 = THREE */
  14, 16,                                  /* 6 = HIGH_LOW */
  -DCT_VAL_CATEGORY1, -DCT_VAL_CATEGORY2,  /* 7 = CAT_ONE */
  18, 20,                                  /* 8 = CAT_THREEFOUR */
  -DCT_VAL_CATEGORY3, -DCT_VAL_CATEGORY4,  /* 9 = CAT_THREE */
  -DCT_VAL_CATEGORY5, -DCT_VAL_CATEGORY6   /* 10 = CAT_FIVE */
};

void TokenCosts::compute_cost( SafeArray<uint16_t, MAX_ENTROPY_TOKENS> & costs_nodes,
                               const SafeArray<Probability, ENTROPY_NODES> & probabilities,
                               size_t tree_index, uint16_t current_cost )
{
  const Probability prob = probabilities.at( tree_index / 2 );

  for ( size_t i = 0; i < 2; i++ ) { // each tree node has two children
    int tree_entry = vp8_coef_tree[ tree_index + i ];
    int16_t cost = current_cost + cost_bit( prob, i );

    if ( tree_entry <= 0 ) { // if it is a zero or negative value, it's a leaf
      costs_nodes.at( -tree_entry ) = cost;
    }
    else { // recursively compute costs for the children of this inner node
      compute_cost( costs_nodes, probabilities, tree_entry, cost );
    }
  }
}

void TokenCosts::fill_costs( const ProbabilityTables & probability_tables )
{
  for ( size_t i = 0; i < BLOCK_TYPES; i++ ) {
    for ( size_t j = 0; j < COEF_BANDS; j++ ) {
      for ( size_t k = 0; k < PREV_COEF_CONTEXTS; k++ ) {
        auto & costs_array = costs.at( i ).at( j ).at( k );
        auto & probabilities = probability_tables.coeff_probs.at( i ).at( j ).at( k );

        if ( k == 0 && j > ( i == 0 ) ) {
          compute_cost( costs_array, probabilities, 2 );
        }
        else {
          compute_cost( costs_array, probabilities );
        }
      }
    }
  }
}
