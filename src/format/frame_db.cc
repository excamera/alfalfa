#include "frame_db.hh"

#include <algorithm>

#include "serialization.hh"
#include "protobufs/alfalfa.pb.h"

using namespace std;

pair<FrameDataSetCollectionByOutputHash::const_iterator, FrameDataSetCollectionByOutputHash::const_iterator>
FrameDB::search_by_output_hash( const size_t output_hash ) const
{
  const FrameDataSetCollectionByOutputHash & data_by_output_hash =
    collection_.get<FrameDataSetByOutputHashTag>();
  return data_by_output_hash.equal_range( output_hash );
}

pair<FrameDataSetSourceHashSearch::const_iterator, FrameDataSetSourceHashSearch::const_iterator>
FrameDB::search_by_decoder_hash( const DecoderHash & decoder_hash ) const
{
  SourceHash query_hash(
    make_optional( true, decoder_hash.state_hash() ),
    make_optional( true, decoder_hash.last_hash() ),
    make_optional( true, decoder_hash.golden_hash() ),
    make_optional( true, decoder_hash.alt_hash() )
  );

  const FrameDataSetCollectionBySourceHash & data_by_source_hash =
    collection_.get<FrameDataSetBySourceHashTag>();
  FrameDataSetSourceHashSearch results( data_by_source_hash, query_hash );

  return make_pair( results.begin(), results.end() );
}

size_t
FrameDB::insert( FrameInfo frame )
{
  size_t returned_frame_id;
  if ( not has_frame_name( frame.name() ) ) {
    returned_frame_id = frame_id_;
    frame_id_++;
    frame.set_frame_id( returned_frame_id );
    collection_.insert( frame );
  } else {
    returned_frame_id = search_by_frame_name( frame.name() ).frame_id();
  }
  return returned_frame_id;
}

bool
FrameDB::has_frame_id( const size_t frame_id ) const
{
  const FrameDataSetCollectionById & index = collection_.get<FrameDataSetByIdTag>();
  return index.count( frame_id ) > 0;
}

bool
FrameDB::has_frame_name( const FrameName & name ) const
{
  const FrameDataSetCollectionByFrameName & index = collection_.get<FrameDataSetByFrameNameTag>();
  return index.count( boost::make_tuple( name.source, name.target ) ) > 0;
}

const FrameInfo &
FrameDB::search_by_frame_id( const size_t frame_id ) const
{
  const FrameDataSetCollectionById & index = collection_.get<FrameDataSetByIdTag>();
  if ( index.count( frame_id ) == 0 )
    throw out_of_range( "Invalid frame_id!" );
  FrameDataSetCollectionById::const_iterator id_iterator =
    index.find( frame_id );
  return *id_iterator;
}

const FrameInfo &
FrameDB::search_by_frame_name( const FrameName & name ) const
{
  const FrameDataSetCollectionByFrameName & index = collection_.get<FrameDataSetByFrameNameTag>();
  auto key = boost::make_tuple( name.source, name.target );
  if ( index.count( key ) == 0 )
    throw out_of_range( "Invalid (source_hash, target_hash) pair" );
  FrameDataSetCollectionByFrameName::const_iterator name_iterator = index.find( key );
  return *name_iterator;
}

FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator::FrameDataSetSourceHashSearchIterator(
  const FrameDataSetCollectionBySourceHash & data_set,
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
  const FrameDataSetCollectionBySourceHash & data_set, SourceHash source_hash )
  : source_hash_( source_hash ), data_set_( data_set ),
    begin_iterator_( data_set, source_hash, false ),
    end_iterator_( data_set, source_hash, true )
{}
