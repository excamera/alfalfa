#include "frame_db.hh"

#include <algorithm>

#include "serialization.hh"
#include "protobufs/alfalfa.pb.h"

void FrameDB::insert( FrameInfo fd )
{
  BasicDatabase::insert( fd );

  if ( fd.ivf_filename().initialized() and
    std::find( ivf_filenames_.begin(), ivf_filenames_.end(),
      fd.ivf_filename().get() ) == ivf_filenames_.end() ) {
    ivf_filenames_.push_back( fd.ivf_filename().get() );
  }
}

// std::pair<FrameDataSetByFrameName::iterator, FrameDataSetByFrameName::iterator>
// FrameDB::search_by_frame_name( std::string & frame_name )
// {
//   FrameDataSetByFrameName & data_by_frame_name = data_.get<0>();
//   return data_by_frame_name.equal_range( frame_name );
// }

std::pair<FrameDataSetByOutputHash::iterator, FrameDataSetByOutputHash::iterator>
FrameDB::search_by_output_hash( const size_t & output_hash )
{
  FrameDataSetByOutputHash & data_by_output_hash = collection_.get<0>();
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

  FrameDataSetBySourceHash & data_by_source_hash = collection_.get<1>();
  FrameDataSetSourceHashSearch results( data_by_source_hash, query_hash );

  return make_pair( results.begin(), results.end() );
}

FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator::FrameDataSetSourceHashSearchIterator(
  FrameDataSetBySourceHash & data_set,
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
  FrameDataSetBySourceHash & data_set, SourceHash source_hash )
  : source_hash_( source_hash ), data_set_( data_set ),
    begin_iterator_( data_set, source_hash, false ),
    end_iterator_( data_set, source_hash, true )
{}
