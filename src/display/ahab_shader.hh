#ifndef AHAB_SHADER_HH
#define AHAB_SHADER_HH

#include "safe_array.hh"

/*
  Matlab inversion of BT.709 matrix:

  >> 255 * inv([219*[.7154 .0721 .2125]' 224*[-.386 .5 -.115]' 224*[-.454 -.046 .5]']')

ans =

   1.164165557121523  -0.213138349939461  -0.532748200973066
   1.166544220758321   2.112430116393991   0.001144179685436
   1.164384394176109   0.000813948963217   1.793155612333230
*/

static const SafeArray< double, 3 > itu709_green = {{ 1.164165557121523,  -0.213138349939461,  -0.532748200973066 }};
static const SafeArray< double, 3 > itu709_blue  = {{ 1.166544220758321,   2.112430116393991,   0.001144179685436 }};
static const SafeArray< double, 3 > itu709_red   = {{ 1.164384394176109,   0.000813948963217,   1.793155612333230 }};

/* Matrix inverstion of SMPTE 170M matrix:

   >> 255 * inv([219*[.587 .114 .299]' 224*[-.331 .500 -.169]' 224*[-.419 -.081 .5]']')

ans =

   1.164383561643836  -0.391260370716072  -0.813004933873461
   1.164383561643836   2.017414758970775   0.001127259960693
   1.164383561643836  -0.001054999706803   1.595670195813386
*/

static const SafeArray< double, 3 > smpte170m_green = {{ 1.164383561643836, -0.391260370716072, -0.813004933873461 }};
static const SafeArray< double, 3 > smpte170m_blue  = {{ 1.164383561643836,  2.017414758970775,  0.001127259960693 }};
static const SafeArray< double, 3 > smpte170m_red   = {{ 1.164383561643836, -0.001054999706803,  1.595670195813386 }};

static char ahab_shader[] = {
  "!!ARBfp1.0\n"

  "ATTRIB where_Y  = fragment.texcoord[0];\n"
  "ATTRIB where_Cb = fragment.texcoord[1];\n"
  "ATTRIB where_Cr = fragment.texcoord[2];\n"
  "OUTPUT out      = result.color;\n"

  "TEMP YPBPR;\n"

  "TEX YPBPR.x, where_Y, texture[0], RECT;\n"
  "TEX YPBPR.y, where_Cb, texture[1], RECT;\n"
  "TEX YPBPR.z, where_Cr, texture[2], RECT;\n"

  "PARAM CTOP = { .06274509803921568627, .50196078431372549019, .50196078431372549019 };\n"

  "SUB YPBPR, YPBPR, CTOP;\n"

  "DP3   out.g, YPBPR, program.local[ 0 ];\n"
  "DP3   out.b, YPBPR, program.local[ 1 ];\n"
  "DP3   out.r, YPBPR, program.local[ 2 ];\n"

  "END\n"
};

#endif /* AHAB_SHADER_HH */
