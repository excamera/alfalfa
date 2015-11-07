#include "frame_db.hh"

#include <algorithm>

#include "serialization.hh"
#include "protobufs/alfalfa.pb.h"

std::pair<FrameDataSetCollectionByOutputHash::iterator, FrameDataSetCollectionByOutputHash::iterator>
FrameDB::search_by_output_hash( const size_t & output_hash )
{
  FrameDataSetCollectionByOutputHash & data_by_output_hash =
    collection_.get<FrameDataSetByOutputHashTag>();
  return data_by_output_hash.equal_range( output_hash );
}

std::pair<FrameDataSetSourceHashSearch::iterator, FrameDataSetSourceHashSearch::iterator>
FrameDB::search_by_decoder_hash( const DecoderHash & decoder_hash )
{
  SourceHash query_hash(
    make_optional( true, decoder_hash.state_hash() ),
    make_optional( true, decoder_hash.continuation_hash() ),
    make_optional( true, decoder_hash.last_hash() ),
    make_optional( true, decoder_hash.golden_hash() ),
    make_optional( true, decoder_hash.alt_hash() )
  );

  FrameDataSetCollectionBySourceHash & data_by_source_hash =
    collection_.get<FrameDataSetBySourceHashTag>();
  FrameDataSetSourceHashSearch results( data_by_source_hash, query_hash );

  return make_pair( results.begin(), results.end() );
}

bool
FrameDB::has_frame_id( const size_t & frame_id )
{
  FrameDataSetCollectionById & index = collection_.get<FrameDataSetByIdTag>();
  return index.count( frame_id ) > 0;
}

bool
FrameDB::has_frame_name( const SourceHash & source_hash, const TargetHash & target_hash )
{
  FrameDataSetCollectionByFrameName & index = collection_.get<FrameDataSetByFrameNameTag>();
  return index.count( boost::make_tuple( source_hash, target_hash ) ) > 0;
}

FrameInfo
FrameDB::search_by_frame_id( const size_t & frame_id )
{
  FrameDataSetCollectionById & index = collection_.get<FrameDataSetByIdTag>();
  if ( index.count( frame_id ) == 0 )
    throw std::out_of_range( "Invalid frame_id!" );
  FrameDataSetCollectionById::iterator id_iterator =
    index.find( frame_id );
  return *id_iterator;
}

FrameInfo
FrameDB::search_by_frame_name( const SourceHash & source_hash, const TargetHash & target_hash )
{
  FrameDataSetCollectionByFrameName & index = collection_.get<FrameDataSetByFrameNameTag>();
  auto key = boost::make_tuple( source_hash, target_hash );
  if ( index.count( key ) == 0 )
    throw std::out_of_range( "Invalid (source_hash, target_hash) pair" );
  FrameDataSetCollectionByFrameName::iterator name_iterator = index.find( key );
  return *name_iterator;
}

void
FrameDB::merge( const FrameDB & db, map<size_t, size_t> & frame_id_mapping, IVFWriter & combined_ivf_writer, const string & ivf_file_path )
{
  IVF ivf_file( ivf_file_path );
  for ( auto item : db.collection_.get<FrameDataSetSequencedTag>() ) {
    if ( has_frame_name( item.source_hash(), item.target_hash() ) ) {
      FrameInfo frame_info = search_by_frame_name( item.source_hash(), item.target_hash() );
      frame_id_mapping[item.frame_id()] = frame_info.frame_id();
    }
    else if ( has_frame_id( item.frame_id() ) ) {
      size_t new_frame_id = item.frame_id();
      while ( not has_frame_id( new_frame_id ) ) {
        new_frame_id++;
      }
      frame_id_mapping[ item.frame_id() ] = new_frame_id;
    } else {
      frame_id_mapping[item.frame_id()] = item.frame_id();
    }

    size_t offset = combined_ivf_writer.append_frame( ivf_file.frame( item.index() ) );
    item.set_offset( offset );
    item.set_frame_id( frame_id_mapping[item.frame_id()] );
    insert( item );
  }

}

FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator::FrameDataSetSourceHashSearchIterator(
  FrameDataSetCollectionBySourceHash & data_set,
  SourceHash source_hash, bool end )
  : stage_( 31 ),
    source_hash_( source_hash ),
    data_set_( data_set ),
    itr_(), begin_(), current_end_()
{
  if ( end ) {
    itr_ = begin_ = current_end_ = data_set_.end();
  }
  else {
    auto result = data_set.equal_range( source_hash );
    itr_ = begin_ = result.first;
    current_end_ = result.second;

    if ( itr_ == current_end_ ) {
      ++( *this );
    }
  }
}

FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator::FrameDataSetSourceHashSearchIterator(
  const FrameDataSetSourceHashSearchIterator & it )
  : stage_( it.stage_ ), source_hash_( it.source_hash_ ), data_set_( it.data_set_ ),
    itr_( it.itr_ ), begin_( it.begin_ ), current_end_( it.current_end_ )
{}

FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator &
FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator::operator++()
{
  if ( itr_ == current_end_ ) {
    if ( stage_ == 0 ) {
      itr_ = current_end_ = begin_ = data_set_.end();
      return *this;
    }

    stage_--;

    SourceHash query_hash(
      make_optional( (stage_ >> 0) & 1, source_hash_.state_hash.get() ),
      make_optional( (stage_ >> 1) & 1, source_hash_.continuation_hash.get() ),
      make_optional( (stage_ >> 2) & 1, source_hash_.last_hash.get() ),
      make_optional( (stage_ >> 3) & 1, source_hash_.golden_hash.get() ),
      make_optional( (stage_ >> 4) & 1, source_hash_.alt_hash.get() )
    );

    auto result = data_set_.equal_range( query_hash );
    itr_ = result.first;
    current_end_ = result.second;

    if ( itr_ == current_end_ ) {
      return ++( *this );
    }
    else {
      return *this;
    }
  }
  else {
    if ( ++itr_ == current_end_ ) {
      return ++( *this );
    }
    else {
      return *this;
    }
  }
}

FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator
FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator::operator++( int )
{
  assert( itr_ != data_set_.end() );

  FrameDataSetSourceHashSearchIterator tmp( *this );
  ++( *this );
  return tmp;
}

bool FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator
::operator==( const FrameDataSetSourceHashSearchIterator & rhs ) const
{
  return itr_ == rhs.itr_;
}

bool FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator
::operator!=( const FrameDataSetSourceHashSearchIterator & rhs ) const
{
  return itr_ != rhs.itr_;
}

const FrameInfo & FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator
::operator*() const
{
  assert( itr_ != data_set_.end() );
  return *itr_;
}

const FrameInfo * FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator
::operator->() const
{
  assert( itr_ != data_set_.end() );
  return &*itr_;
}

FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearch(
  FrameDataSetCollectionBySourceHash & data_set, SourceHash source_hash )
  : source_hash_( source_hash ), data_set_( data_set ),
    begin_iterator_( data_set, source_hash, false ),
    end_iterator_( data_set, source_hash, true )
{}
