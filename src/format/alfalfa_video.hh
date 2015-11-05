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

#define FRAME_DB_FILENAME "frame.db"
#define RASTER_LIST_FILENAME "raster.db"
#define QUALITY_DB_FILENAME "quality.db"
#define TRACK_DB_FILENAME "track.db"
#define SWITCH_DB_FILENAME "switch.db"
#define VIDEO_MANIFEST_FILENAME "video.manifest"
#define IVF_FILENAME "v"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "db.hh"
#include "ivf.hh"
#include "player.hh"
#include "frame_db.hh"
#include "manifests.hh"
#include "ivf_writer.hh"

class AlfalfaVideo
{
public:
  class VideoDirectory
  {
  private:
    std::string directory_path_;

  public:
    VideoDirectory( const std::string & name );

    const std::string & path() const { return directory_path_; }
    std::string video_manifest_filename() const;
    std::string raster_list_filename() const;
    std::string quality_db_filename() const;
    std::string frame_db_filename() const;
    std::string track_db_filename() const;
    std::string switch_db_filename() const;
    std::string ivf_filename() const;
  };

protected:
  VideoDirectory directory_;
  VideoManifest video_manifest_;
  RasterList raster_list_;
  QualityDB quality_db_;
  FrameDB frame_db_;
  TrackDB track_db_;
  SwitchDB switch_db_;
  std::map<std::pair<size_t, size_t>, std::unordered_set<size_t>> switch_mappings_;

  AlfalfaVideo( const std::string & directory_name, OpenMode mode = OpenMode::READ );

public:
  /* Checks if it's possible to merge with the given alfalfa video. */
  bool can_combine( const AlfalfaVideo & video );

  VideoManifest & video_manifest() { return video_manifest_; }
  const VideoManifest video_manifest() const { return video_manifest_; }

  const VideoDirectory & directory() const { return directory_; }

  RasterList & raster_list() { return raster_list_; }
  const RasterList & raster_list() const { return raster_list_; }

  QualityDB & quality_db() { return quality_db_; }
  const QualityDB & quality_db() const { return quality_db_; }

  FrameDB & frame_db() { return frame_db_; }
  const FrameDB & frame_db() const { return frame_db_; }

  TrackDB & track_db() { return track_db_; }
  const TrackDB & track_db() const { return track_db_; }

  SwitchDB & switch_db() { return switch_db_; }
  const SwitchDB & switch_db() const { return switch_db_; }

  /* Gets an iterator over all available track ids. */
  std::pair<std::unordered_set<size_t>::iterator, std::unordered_set<size_t>::iterator>
  get_track_ids();

  /* Gets an iterator over all available track ids that we can switch to from
     the provided track and frame_index. */
  std::pair<std::unordered_set<size_t>::iterator, std::unordered_set<size_t>::iterator>
  get_track_ids_for_switch( const size_t from_track_id, const size_t from_frame_index );

  /* Gets an iterator over all frames in the alf video's frame db. */
  std::pair<FrameDataSetCollectionSequencedAccess::iterator, FrameDataSetCollectionSequencedAccess::iterator> get_frames( void ) const;

  /* Gets an iterator over all frames associated with the particular track. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const size_t track_id );

  /* Gets an iterator over all remaining frames in the track associated with
     the passed in TrackDBIterator. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const TrackDBIterator & it );

  /* Gets an iterator over all remaining frames in the new track once the switch
     implied by the passed-in iterator is applied. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const SwitchDBIterator & it );

  /* Gets an iterator over all frames associated with the switch from the current
     track and frame_index position to the provided track. */
  std::pair<SwitchDBIterator, SwitchDBIterator> get_frames( const TrackDBIterator & it,
                                                            const size_t to_track_id );

  /* Get the quality of the frame associated with supplied frame_info, given
     position in the original raster list. */
  double get_quality( int raster_index, const FrameInfo & frame_info );

  bool good() const;

  AlfalfaVideo( const AlfalfaVideo & other ) = delete;
  AlfalfaVideo & operator=( const AlfalfaVideo & other ) = delete;

  AlfalfaVideo( AlfalfaVideo && other );
};

class PlayableAlfalfaVideo : public AlfalfaVideo
{
private:
  File ivf_file_;

public:
  PlayableAlfalfaVideo( const std::string & directory_name );

  const Chunk get_chunk( const FrameInfo & frame_info ) const;
  void encode( const size_t track_id, std::vector<std::string> vpxenc_args,
               const std::string & destination );
};

class WritableAlfalfaVideo : public AlfalfaVideo
{
private:
  IVFWriter ivf_writer_;

  void insert_frame( FrameInfo next_frame,
                     const size_t original_raster, const double quality,
                     const size_t track_id );

public:
  WritableAlfalfaVideo( const std::string & directory_name,
                        const std::string & fourcc, const uint16_t width, const uint16_t height );
  WritableAlfalfaVideo( const std::string & directory_name, const IVF & ivf );
  WritableAlfalfaVideo( const std::string & directory_name, const VideoInfo & info );

  /* Combine the provided video with the current video. Makes sure frame ids /
     track ids / switch ids don't conflict. */
  void combine( const PlayableAlfalfaVideo & video );
  /* Convert supplied ivf file into alfalfa video. */
  void import( const std::string & filename );
  /* Import function used to create an encoded video. */
  void import( const std::string & filename, PlayableAlfalfaVideo & original,
               const size_t ref_track_id = 0 );
  /* Insert the provided frames into the switch db. */
  void insert_switch_frames( const TrackDBIterator & origin_iterator,
                             const std::vector<FrameInfo> & frames,
                             const TrackDBIterator & dest_iterator );

  FrameInfo import_serialized_frame( const SerializedFrame & frame );

  bool save();
};

#endif /* ALFALFA_VIDEO_HH */
