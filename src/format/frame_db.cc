#include "frame_db.hh"

#include <algorithm>

#include "serialization.hh"
#include "protobufs/alfalfa.pb.h"

vector<std::string> FrameDB::ivf_files()
{
  set<std::string> ivf_filenames;

  for ( auto const & it : collection_.get<FrameDataSetSequencedTag>() ) {
    if ( it.ivf_filename().initialized() and ivf_filenames.count( it.ivf_filename().get() ) == 0 ) {
      ivf_filenames.insert( it.ivf_filename().get() );
    }
  }

  return std::vector<std::string>( ivf_filenames.begin(), ivf_filenames.end() );
}

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
