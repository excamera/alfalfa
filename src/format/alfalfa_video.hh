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
  /* Getter for video info. */
  const VideoInfo & get_info() const { return video_manifest_.info(); }

  /* Getters for raster data. */
  size_t get_raster_list_size( void ) const;
  RasterData get_raster( const size_t raster_index ) const;
  bool has_raster( const size_t raster ) const;
  bool equal_raster_lists( const AlfalfaVideo & video ) const;

  /* Gets an iterator over all quality data in the alf video's quality db. */
  std::pair<QualityDBCollectionSequencedAccess::iterator, QualityDBCollectionSequencedAccess::iterator>
  get_quality_data( void ) const;

  /* Gets an iterator over quality data by approximate raster. */
  std::pair<QualityDBCollectionByApproximateRaster::iterator, QualityDBCollectionByApproximateRaster::iterator>
  get_quality_data_by_approximate_raster( const size_t approximate_raster ) const;

  /* Gets an iterator over quality data by original raster. */
  std::pair<QualityDBCollectionByOriginalRaster::iterator, QualityDBCollectionByOriginalRaster::iterator>
  get_quality_data_by_original_raster( const size_t original_raster ) const;

  /* Gets an iterator over all frames in the alf video's frame db. */
  std::pair<FrameDataSetCollectionSequencedAccess::iterator, FrameDataSetCollectionSequencedAccess::iterator>
  get_frames( void ) const;

  /* Gets an iterator over all track data in the alf video's track db. */
  std::pair<TrackDBCollectionSequencedAccess::iterator, TrackDBCollectionSequencedAccess::iterator>
  get_track_data( void ) const;

  /* Gets an iterator over all switch data in the alf video's switch db. */
  std::pair<SwitchDBCollectionSequencedAccess::iterator, SwitchDBCollectionSequencedAccess::iterator>
  get_switch_data( void ) const;

  /* Checks if it's possible to merge with the given alfalfa video. */
  bool can_combine( const AlfalfaVideo & video ) const;

  /* Checks if alfalfa video has the given track. */
  bool has_track( const size_t track_id ) const { return track_db_.has_track( track_id ); }

  /* Gets an iterator over all available track ids. */
  std::pair<std::unordered_set<size_t>::const_iterator, std::unordered_set<size_t>::const_iterator>
  get_track_ids() const;

  /* Gets an iterator over all available track ids that we can switch to from
     the provided track and frame_index. */
  std::pair<std::unordered_set<size_t>::iterator, std::unordered_set<size_t>::iterator>
  get_track_ids_for_switch( const size_t from_track_id, const size_t from_frame_index ) const;

  /* Gets an iterator over all frames  by output hash. */
  std::pair<FrameDataSetCollectionByOutputHash::iterator, FrameDataSetCollectionByOutputHash::iterator>
  get_frames_by_output_hash( const size_t output_hash ) const;

  /* Determines if alfalfa video has a frame with provided name. */
  bool
  has_frame_name( const FrameName & name ) const { return frame_db_.has_frame_name( name ); }

  /* Returns FrameInfo associated with provided name. */
  const FrameInfo &
  get_frame( const FrameName & name ) const { return frame_db_.search_by_frame_name( name ); }

  /* Gets an iterator over all frames associated with the particular track. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const size_t track_id ) const;

  /* Gets an iterator over all remaining frames in the track associated with
     the passed in TrackDBIterator. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const TrackDBIterator & it ) const;

  /* Gets an iterator over all remaining frames in the new track once the switch
     implied by the passed-in iterator is applied. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const SwitchDBIterator & it ) const;

  /* Gets an iterator over all frames associated with the switch from the current
     track and frame_index position to the provided track. */
  std::pair<SwitchDBIterator, SwitchDBIterator> get_frames( const TrackDBIterator & it,
                                                            const size_t to_track_id ) const;

  /* Get the quality of the frame associated with supplied frame_info, given
     position in the original raster list. */
  double get_quality( int raster_index, const FrameInfo & frame_info ) const;

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
               const std::string & destination ) const;
};

class WritableAlfalfaVideo : public AlfalfaVideo
{
private:
  IVFWriter ivf_writer_;

public:
  WritableAlfalfaVideo( const std::string & directory_name,
                        const std::string & fourcc, const uint16_t width, const uint16_t height );

  /* Insert helper methods. Each of these methods preserve the alf video's invariants. */
  void insert_frame( FrameInfo next_frame,
                     const size_t original_raster, const double quality,
                     const size_t track_id );
  size_t append_frame_to_ivf( Chunk chunk ) { return ivf_writer_.append_frame( chunk ); }
  void insert_raster( RasterData data ) { raster_list_.insert( data ); } 
  void insert_quality_data( QualityData data ) { quality_db_.insert( data ); }
  void insert_track_data( TrackData data ) { track_db_.insert( data ); }
  void insert_switch_data( SwitchData data ) { switch_db_.insert( data ); }
  size_t insert_frame_data( FrameInfo data, const Chunk & chunk );
  /* Insert the provided frames into the switch db. */
  void insert_switch_frames( const TrackDBIterator & origin_iterator,
                             const std::vector<FrameInfo> & frames,
                             const TrackDBIterator & dest_iterator );
  FrameInfo import_serialized_frame( const SerializedFrame & frame );

  bool save();
};

void combine( WritableAlfalfaVideo& combined_video,
              const PlayableAlfalfaVideo & video );

#endif /* ALFALFA_VIDEO_HH */
