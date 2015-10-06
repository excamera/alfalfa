#ifndef MANIFESTS_HH
#define MANIFESTS_HH

#include <string>

/* sequence of hashes of the original rasters that the video approximates */

class RasterList
{
public:
  RasterList( const std::string & filename );
};

/* relation:
   original raster / approximate raster / quality */

class QualityDB
{
public:
  QualityDB( const std::string & filename );
};

/* relation:
   frame name / IVF filename / offset / length */

class FrameDB
{
public:
  FrameDB( const std::string & filename );
};

/* relation:
   ID => { sequence of frame names } */

class TrackDB
{
public:
  TrackDB( const std::string & filename );
};

#endif /* MANIFESTS_HH */
