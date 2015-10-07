#ifndef MANIFESTS_HH
#define MANIFESTS_HH

#include <string>

#include "database.hh"

/* sequence of hashes of the original rasters that the video approximates */

class RasterList
{
public:
  RasterList( const std::string & filename );
};

struct QualityData
{
  size_t approximate_raster;
  double quality;
};

struct FrameData
{
  std::string ivf_filename;
  uint64_t offset;
  uint64_t length;
};

class TrackData : public vector<std::string>
{};

/* relation:
   original raster / approximate raster / quality */

using QualityDB = Database< size_t, QualityData >;

/* relation:
   frame name / IVF filename / offset / length */

using FrameDB = Database< size_t, FrameData >;

/* relation:
   ID => { sequence of frame names } */

using TrackDB = Database< size_t, TrackData >;

#endif /* MANIFESTS_HH */
