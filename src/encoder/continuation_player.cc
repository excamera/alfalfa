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

  StateUpdateFrame state_frame( source.decoder_.get_state().probability_tables,
                                decoder_.get_state().probability_tables );

  DecoderState new_state = source.decoder_.get_state();
  vector<uint8_t> raw = state_frame.serialize( new_state.probability_tables );
  new_state.probability_tables.update( state_frame.header() );

  assert( new_state.probability_tables == decoder_.get_state().probability_tables );

  SourceHash source_hash = source.decoder_.source_hash( DependencyTracker { true, false, false, false } );
  TargetHash target_hash( UpdateTracker( false, false, false, false, false, false, false ),
                          new_state.hash(), 0, false );

  
  return SerializedFrame( FrameName( source_hash, target_hash ), raw );
}

#ifndef NDEBUG

template <class FrameType>
static bool correctly_serialized( const Decoder & orig_decoder,
                                  const vector<uint8_t> & raw_frame,
                                  const FrameType & frame )
{
  Decoder decoder( orig_decoder );

  FrameType new_frame = decoder.parse_frame<FrameType>( decoder.decompress_frame( Chunk( raw_frame.data(), raw_frame.size() ) ) );

  return frame == new_frame;
}

static bool correct_output( const FramePlayer & source, const vector<uint8_t> & raw, const RasterHandle & orig_output )
{
  FramePlayer player( source );

  Optional<RasterHandle> output = player.decode( Chunk( raw.data(), raw.size() ) );

  const VP8Raster & correct = orig_output.get();
  const VP8Raster & real = output.get().get();

  for ( unsigned row = 0; row < correct.height() / 16; row++ ) {
    for ( unsigned col = 0; col < correct.width() / 16; col++ ) {
      if ( not ( correct.macroblock( col, row ) == real.macroblock( col, row ) ) ) {
        cerr << "Incorrect macroblock at: " << col << " " << row << "\n";
        return false;
      }
    }
  }

  return true;
}

#endif

static SerializedFrame make_partial_ref( const reference_frame & ref_frame,
                                         const ReferenceDependency & deps,
                                         const Decoder & source,
                                         const References & target_refs )
{

  ReferenceUpdater updater( ref_frame, source.get_references().at( ref_frame ),
                            target_refs.at( ref_frame ), deps );

  RefUpdateFrame ref_update( ref_frame, updater );

  vector<uint8_t> raw_frame = ref_update.serialize( source.get_state().probability_tables );

  assert( correctly_serialized( source, raw_frame, ref_update ) );

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

  ReferenceDependency dependencies( inter_frame_.get().macroblocks() );

  MissingTracker missing = source.decoder_.find_missing( prev_references_, dependencies );

  vector<SerializedFrame> frames;

  if ( missing.last and used.need_last ) {
    frames.push_back( make_partial_ref( LAST_FRAME, dependencies, source.decoder_, prev_references_ ) );
  }

  if ( missing.golden and used.need_golden ) {
    frames.push_back( make_partial_ref( GOLDEN_FRAME, dependencies, source.decoder_, prev_references_ ) );
  }

  if ( missing.alternate and used.need_alternate ) {
    frames.push_back( make_partial_ref( ALTREF_FRAME, dependencies, source.decoder_, prev_references_ ) );
  }

  return frames;
}

SerializedFrame ContinuationPlayer::rewrite_inter_frame( const SourcePlayer & source ) const
{
  // Should only be making continuations off a shown frame
  assert( shown_ );
  // A key frame will have been written out anyway and can be used by anyone
  assert( not on_key_frame_ );

  InterFrame inter( inter_frame_.get(),
                    decoder_.get_state().segmentation, decoder_.get_state().filter_adjustments );

  vector<uint8_t> raw = inter.serialize( source.decoder_.get_state().probability_tables );

  assert( correctly_serialized( source.decoder_, raw, inter ) );
  assert( correct_output( source, raw, cur_output_ ) );

  // Checks if we correctly analyzed the dependencies of the inter frame's
  // macroblocks. Slow.
  //inter.analyze_dependencies( ReferenceDependency( inter.macroblocks() ) );

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
