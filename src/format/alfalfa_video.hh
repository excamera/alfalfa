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

#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "db.hh"
#include "ivf.hh"
#include "player.hh"
#include "frame_db.hh"
#include "manifests.hh"
#include "ivf_writer.hh"

enum class OpenMode
{
  READ,
  Create,
};

class AlfalfaVideo
{
public:
  class VideoDirectory
  {
  private:
    OpenMode mode_;
    
    FileDescriptor directory_;

    FileDescriptor subfile( const std::string & filename ) const;

  public:
    VideoDirectory( const std::string & name, const OpenMode mode );

    FileDescriptor video_manifest() const;
    FileDescriptor raster_list() const;
    FileDescriptor quality_db() const;
    FileDescriptor frame_db() const;
    FileDescriptor track_db() const;
    FileDescriptor switch_db() const;
    FileDescriptor ivf_file() const;

    const std::string & ivf_filename() const;
  };

private:
  void initialize_dri_to_frame_index_mapping();

protected:
  VideoDirectory directory_;
  VideoManifest video_manifest_;
  RasterList raster_list_ {};
  QualityDB quality_db_ {};
  FrameDB frame_db_ {};
  TrackDB track_db_ {};
  SwitchDB switch_db_ {};

   /* Required since different tracks can have different number of frames;
     this is to ensure that we can easily line up different tracks according to
     displayed raster. (here dri stands for displayed_raster_index).
     This is a map from track_id to (map from dri to vector of all frame_indies) */
  std::unordered_map<size_t, std::unordered_map<size_t, std::vector<size_t>>> dri_to_frame_index_mapping_ {};

  AlfalfaVideo( const uint16_t width, const uint16_t height,
		const std::string & name ); /* new blank video */
  AlfalfaVideo( const std::string & name ); /* open existing video for reading */

public:
  /* Getter for video info. */
  const VideoInfo & get_info() const { return video_manifest_.info(); }

  /* Getters for raster data. */
  size_t get_raster_list_size( void ) const;
  RasterData get_raster( const size_t raster_index ) const;
  bool has_raster( const size_t raster ) const;
  bool equal_raster_lists( const AlfalfaVideo & video ) const;

  /* Access dri_to_frame_index_mapping. */
  std::vector<size_t> get_dri_to_frame_index_mapping( const size_t track_id, const size_t dri ) const;

  /* Gets an iterator over all quality data in the alf video's quality db, in no particular
     order. */
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

  /* Get number of frames in given track. */
  size_t get_track_size( const size_t track_id ) const { return track_db_.get_end_frame_index( track_id ); }

  /* Gets an iterator over all switch data in the alf video's switch db, again in no
     particular order. */
  std::pair<SwitchDBCollectionSequencedAccess::iterator, SwitchDBCollectionSequencedAccess::iterator>
  get_switch_data( void ) const;

  /* Checks if it's possible to merge with the given alfalfa video. */
  bool can_combine( const AlfalfaVideo & video ) const;

  /* Checks if alfalfa video has the given track. */
  bool has_track( const size_t track_id ) const { return track_db_.has_track( track_id ); }

  /* Gets an iterator over all available track ids, in no particular order. */
  std::pair<std::unordered_set<size_t>::const_iterator, std::unordered_set<size_t>::const_iterator>
  get_track_ids() const;

  std::pair<TrackDBCollectionByFrameIdIndex::const_iterator, TrackDBCollectionByFrameIdIndex::const_iterator>
  get_track_data_by_frame_id( const size_t frame_id ) const { return track_db_.search_by_frame_id( frame_id ); }

  /* Gets an iterator over all frames  by output hash / decoder hash. */
  std::pair<FrameDataSetCollectionByOutputHash::iterator, FrameDataSetCollectionByOutputHash::iterator>
  get_frames_by_output_hash( const size_t output_hash ) const;
  std::pair<FrameDataSetSourceHashSearch::iterator, FrameDataSetSourceHashSearch::iterator>
  get_frames_by_decoder_hash( const DecoderHash & decoder_hash ) const;

  /* Determines if alfalfa video has a frame with provided name. */
  bool
  has_frame_name( const FrameName & name ) const { return frame_db_.has_frame_name( name ); }

  /* Returns FrameInfo associated with provided name / id. */
  const FrameInfo &
  get_frame( const FrameName & name ) const { return frame_db_.search_by_frame_name( name ); }
  const FrameInfo &
  get_frame( const size_t frame_id ) const { return frame_db_.search_by_frame_id( frame_id ); }
  const TrackData &
  get_frame( const size_t track_id, const size_t frame_index ) const { return track_db_.get_frame( track_id, frame_index ); }

  /* Gets an iterator over all frames associated with the particular track. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const size_t track_id ) const;

  /* Gets an iterator over all frames in the track associated with
     the passed-in TrackDBIterator, starting at the position represented
     by the passed-in TrackDBIterator. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const TrackDBIterator & it ) const;

  /* Gets an iterator over all remaining frames in the new track once the switch
     implied by the passed-in iterator is applied; starts at the position represented
     by the passed-in SwitchDBIterator. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames( const SwitchDBIterator & it ) const;

  /* Gets an iterator over all frames associated with the switch from the current
     track and frame_index position to the provided track. */
  std::pair<SwitchDBIterator, SwitchDBIterator> get_frames( const TrackDBIterator & it,
                                                            const size_t to_track_id ) const;

  /* Gets an iterator over all frames associated with a switch, given from_track_id, to_track_id
     and a frame_index position explicitly. */
  std::pair<SwitchDBIterator, SwitchDBIterator> get_frames_switch( const size_t from_track_id,
                                                                   const size_t from_frame_index,
                                                                   const size_t to_track_id ) const;

  /* Gets an iterator in the track starting from the given frame, backwards
     to the beginning; here the frame with index frame_index (includes unshown frames)
     is _included_. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames_reverse( const size_t track_id,
                                                                  const size_t frame_index ) const;
  /* Here, from_frame_index and to_frame_index (index includes unshown frames)
     are both _included_. */
  std::pair<TrackDBIterator, TrackDBIterator> get_frames_reverse( const size_t track_id,
                                                                  const size_t from_frame_index,
                                                                  const size_t to_frame_index ) const;

  /* Gets all switches ending with given frame. */
  std::vector<std::pair<SwitchDBIterator, SwitchDBIterator> >
  get_switches_ending_with_frame( const size_t frame_id ) const;

  /* Gets an iterator over all frames associated with a track, starting at
     start_frame_index and ending at end_frame_index (both indices here include
     unshown frames) -- frame at end_frame_index is NOT included. */
  std::pair<TrackDBIterator, TrackDBIterator> get_track_range( const size_t track_id,
                                                               const size_t start_frame_index,
                                                               const size_t end_frame_index ) const;

  /* Gets an iterator over all frames associated with a switch -- including the frame
     at index switch_start_index within the switch and NOT including the frame at index
     switch_end_index within the switch. From_frame_index is the index of the frame
     on the starting track where the switch starts. */
  std::pair<SwitchDBIterator, SwitchDBIterator> get_switch_range( const size_t from_track_id,
                                                                  const size_t to_track_id,
                                                                  const size_t from_frame_index,
                                                                  const size_t switch_start_index,
                                                                  const size_t switch_end_index ) const;

  /* Get the quality of the frame associated with supplied frame_info, given
     the index of the corresponding original raster in the raster_list. */
  double get_quality( int raster_index, const FrameInfo & frame_info ) const;

  /* the Web server wants to redirect incoming requests to this file */
  const std::string & ivf_filename() const;
  
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
  /* Keeps track of the last allocated displayed_raster_index for each track. */
  std::unordered_map<size_t, size_t> track_displayed_raster_indices_ {};

public:
  WritableAlfalfaVideo( const std::string & directory_name,
                        const uint16_t width, const uint16_t height );

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

  void save() const;
};

void combine( WritableAlfalfaVideo& combined_video,
              const PlayableAlfalfaVideo & video );

#endif /* ALFALFA_VIDEO_HH */
