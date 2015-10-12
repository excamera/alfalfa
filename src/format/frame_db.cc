#include "frame_db.hh"

FrameDB::FrameDB( const std::string & filename )
  : data_()
{
  std::cout << filename << endl;
}

void FrameDB::insert( FrameData fd )
{
  data_.insert( fd );
}

std::pair<FrameDataSetByFrameName::iterator, FrameDataSetByFrameName::iterator>
FrameDB::search_by_frame_name( std::string & frame_name )
{
  FrameDataSetByFrameName & data_by_frame_name = data_.get<0>();
  return data_by_frame_name.equal_range( frame_name );
}

std::pair<FrameDataSetByOutputHash::iterator, FrameDataSetByOutputHash::iterator>
FrameDB::search_by_output_hash( const size_t & output_hash )
{
  FrameDataSetByOutputHash & data_by_output_hash = data_.get<1>();
  return data_by_output_hash.equal_range( output_hash );
}

std::pair<FrameDataSetSourceHashSearch::iterator, FrameDataSetSourceHashSearch::iterator>
FrameDB::search_by_source_hash( const size_t & state_hash, const size_t & continuation_hash,
  const size_t & last_hash, const size_t & golden_hash, const size_t & alt_hash )
{
  SourceHash query_hash(
    make_optional( true, state_hash ),
    make_optional( true, continuation_hash ),
    make_optional( true, last_hash ),
    make_optional( true, golden_hash ),
    make_optional( true, alt_hash )
  );

  FrameDataSetBySourceHash & data_by_source_hash = data_.get<2>();
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
  assert( itr_ != data_set_.end() );

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

const FrameData & FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator
::operator*() const
{
  assert( itr_ != data_set_.end() );
  return *itr_;
}

const FrameData & FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearchIterator
::operator->() const
{
  assert( itr_ != data_set_.end() );
  return *itr_;
}

FrameDataSetSourceHashSearch::FrameDataSetSourceHashSearch(
  FrameDataSetBySourceHash & data_set, SourceHash source_hash )
  : source_hash_( source_hash ), data_set_( data_set ),
    begin_iterator_( data_set, source_hash, false ),
    end_iterator_( data_set, source_hash, true )
{}
