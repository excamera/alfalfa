#include "continuation_player.hh"
#include "uncompressed_chunk.hh"

using namespace std;

ContinuationPlayer::ContinuationPlayer( const uint16_t width, const uint16_t height )
  : FramePlayer( width, height ),
    prev_references_( width, height ),
    shown_( false ),
    cur_output_( prev_references_.last ),
    on_key_frame_( true ),
    key_frame_(),
    inter_frame_()
{}

template<class FrameType>
Optional<RasterHandle> ContinuationPlayer::track_continuation_info( const FrameType & frame )
{
  prev_references_ = decoder_.get_references();

  tie( shown_, cur_output_ ) = decoder_.decode_frame( frame );

  return make_optional( shown_, cur_output_ );
}

Optional<RasterHandle> ContinuationPlayer::decode( const Chunk & chunk )
{
  UncompressedChunk decompressed_frame = decoder_.decompress_frame( chunk );

  if ( decompressed_frame.key_frame() ) {
    on_key_frame_ = true;
    key_frame_ = decoder_.parse_frame<KeyFrame>( decompressed_frame );

    return track_continuation_info( key_frame_.get() );
  } else {
    on_key_frame_ = false;
    inter_frame_ = decoder_.parse_frame<InterFrame>( decompressed_frame );

    return track_continuation_info( inter_frame_.get() );
  }
}

bool ContinuationPlayer::on_key_frame( void ) const
{
  return on_key_frame_;
}

SerializedFrame ContinuationPlayer::make_state_update( const SourcePlayer & source ) const
{
  assert( not on_key_frame_ );

  /* FIXME probably remove the diff */
  DecoderDiff diff = decoder_ - source.decoder_;

  StateUpdateFrame state_frame( diff.source_probabilities, diff.target_probabilities );

  DecoderState new_state = source.decoder_.get_state();
  new_state.probability_tables.mv_prob_replace( state_frame.header().mv_prob_replacement );

  vector<uint8_t> raw = state_frame.serialize( diff.target_probabilities );

  SourceHash source_hash = source.decoder_.source_hash( DependencyTracker { true, false, false, false } );
  TargetHash target_hash( UpdateTracker( false, false, false, false, false, false, false ),
                          new_state.hash(), 0, false );

  
  return SerializedFrame( FrameName( source_hash, target_hash ), raw );
}

static SerializedFrame make_partial_ref( const reference_frame & ref_frame,
                                    const ReferenceDependency & deps,
                                    const Decoder & source,
                                    const Decoder & target )
{
  ReferenceUpdater updater( ref_frame, source.get_references().last,
                            target.get_references().last, deps );

  RefUpdateFrame ref_update( ref_frame, updater );

  vector<uint8_t> raw_frame = ref_update.serialize( source.get_state().probability_tables );

  SourceHash source_hash = source.source_hash( DependencyTracker { true, ref_frame == LAST_FRAME,
                                                                   ref_frame == GOLDEN_FRAME,
                                                                   ref_frame == ALTREF_FRAME } );
  TargetHash target_hash( ref_update.get_updated(), source.get_hash().state_hash(), 
                          updater.new_reference().hash(), false );


  return SerializedFrame( FrameName( source_hash, target_hash ), raw_frame );
}

vector<SerializedFrame> ContinuationPlayer::make_reference_updates( const SourcePlayer & source ) const
{
  assert( not on_key_frame_ );

  DependencyTracker used = inter_frame_.get().get_used();
  MissingTracker missing = source.decoder_.find_missing( prev_references_ );

  ReferenceDependency dependencies( inter_frame_.get().macroblocks() );

  vector<SerializedFrame> frames;

  if ( missing.last and used.need_last ) {
    frames.push_back( make_partial_ref( LAST_FRAME, dependencies, source.decoder_, decoder_ ) );
  }

  if ( missing.golden and used.need_golden ) {
    frames.push_back( make_partial_ref( GOLDEN_FRAME, dependencies, source.decoder_, decoder_ ) );
  }

  if ( missing.alternate and used.need_alternate ) {
    frames.push_back( make_partial_ref( ALTREF_FRAME, dependencies, source.decoder_, decoder_ ) );
  }

  return frames;
}

SerializedFrame ContinuationPlayer::rewrite_inter_frame( const SourcePlayer & source ) const
{
  DecoderDiff difference = decoder_ - source.decoder_;

  // Should only be making continuations off a shown frame
  assert( shown_ );
  // A key frame will have been written out anyway and can be used by anyone
  assert( not on_key_frame_ );

  InterFrame inter( inter_frame_.get(),
                    difference.source_probabilities, difference.target_probabilities,
                    difference.target_segmentation, difference.target_filter );

  vector<uint8_t> raw = inter.serialize( difference.target_probabilities );

  return SerializedFrame( FrameName( SourceHash( source.decoder_.source_hash( inter.get_used() ) ),
                                     TargetHash( decoder_.target_hash( inter.get_updated(), cur_output_,
                                                 inter.show_frame() ) ) ),
                          raw );
}

void ContinuationPlayer::apply_changes( SourcePlayer & source ) const
{
  source.decoder_.apply_decoded_frame( inter_frame_.get(), cur_output_, decoder_ );
}

bool ContinuationPlayer::need_state_update( const SourcePlayer & source ) const
{
  return not ( decoder_.get_state() == source.decoder_.get_state() );
}
