#ifndef ALFALFA_VIDEO_HH
#define ALFALFA_VIDEO_HH

/* object that represents a .alf "video directory"
   on disk.

   The components are:

   1. one or more IVF files containing coded frames
   2. a "raster list" of hashes of the original
      rasters that the video approximates
   3. a "quality database" of the relation
      original raster / approximate raster / quality
   4. a "frame database" of the relation
      frame name / filename / offset / length
   5. a "trajectory database" of the relation
      ID => { list of frame names }
   
*/

#include <unordered_map>

#include "ivf.hh"
#include "manifests.hh"

class AlfalfaVideo
{
private:
  class VideoDirectory
  {
  public:
    VideoDirectory( const std::string & name );

    std::string raster_list_filename() const;
    std::string quality_db_filename() const;
    std::string frame_db_filename() const;
    std::string trajectory_db_filename() const;
  };

  VideoDirectory directory_;
  std::unordered_map<std::string, IVF> video_files_;
  RasterList raster_list_;
  QualityDB quality_db_;
  FrameDB frame_db_;
  TrajectoryDB trajectory_db_;

public:
  AlfalfaVideo( const std::string & directory_name );

  void add_ivf_file( std::string & name, const IVF & file );

  RasterList & raster_list() { return raster_list_; }
  const RasterList & raster_list() const { return raster_list_; }

  QualityDB & quality_db() { return quality_db_; }
  const QualityDB & quality_db() const { return quality_db_; }

  FrameDB & frame_db() { return frame_db_; }
  const FrameDB & frame_db() const { return frame_db_; }

  TrajectoryDB & trajectory_db() { return trajectory_db_; }
  const TrajectoryDB & trajectory_db() const { return trajectory_db_; }
};

#endif /* ALFALFA_VIDEO_HH */
