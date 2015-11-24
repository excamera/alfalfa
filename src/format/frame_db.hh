#ifndef FRAME_DB_HH
#define FRAME_DB_HH

#include <map>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/tag.hpp>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "db.hh"
#include "optional.hh"
#include "frame_info.hh"
#include "dependency_tracking.hh"
#include "protobufs/alfalfa.pb.h"
#include "ivf_writer.hh"
#include "filesystem.hh"

using namespace boost::multi_index;
using boost::multi_index_container;

using namespace google::protobuf::io;

struct FrameData_OutputHashExtractor
{
  typedef const size_t result_type;

  const result_type & operator()( const FrameInfo & fd ) const { return fd.target_hash().output_hash; }
  result_type & operator()( FrameInfo * fd ) { return fd->target_hash().output_hash; }
};

struct FrameData_SourceHashExtractor
{
  typedef const SourceHash result_type;

  const result_type & operator()( const FrameInfo & fd ) const { return fd.source_hash(); }
  result_type & operator()( FrameInfo * fd ) { return fd->source_hash(); }
};

struct FrameData_TargetHashExtractor
{
  typedef const TargetHash result_type;

  const result_type operator()( const FrameInfo & fd ) const { return fd.target_hash(); }
  result_type & operator()( FrameInfo * fd ) { return fd->target_hash(); }
};

struct FrameData_FrameIdExtractor
{
  typedef const size_t result_type;

  const result_type & operator()( const FrameInfo & fd ) const { return fd.frame_id(); }
  result_type & operator()( FrameInfo * fd ) { return fd->frame_id(); }
};

/*
 * FrameDataSet
 *  sequence of frames contained in various tracks in given alf video
 */

struct FrameDataSetByIdTag;
struct FrameDataSetByOutputHashTag;
struct FrameDataSetBySourceHashTag;
struct FrameDataSetByFrameNameTag;
struct FrameDataSetSequencedTag;

typedef multi_index_container
<
  FrameInfo,
  indexed_by
  <
    hashed_unique
    <
      tag<FrameDataSetByIdTag>,
      FrameData_FrameIdExtractor
    >,
    hashed_non_unique
    <
      tag<FrameDataSetByFrameNameTag>,
      composite_key
      <
        FrameInfo,
        FrameData_SourceHashExtractor,
        FrameData_TargetHashExtractor
      >,
      composite_key_hash
      <
        std::hash<SourceHash>,
        std::hash<TargetHash>
      >,
      composite_key_equal_to
      <
        std::equal_to<SourceHash>,
        std::equal_to<TargetHash>
      >
    >,
    hashed_non_unique
    <
      tag<FrameDataSetByOutputHashTag>,
      FrameData_OutputHashExtractor
    >,
    hashed_non_unique
    <
      tag<FrameDataSetBySourceHashTag>,
      FrameData_SourceHashExtractor,
      std::hash<SourceHash>,
      std::equal_to<SourceHash>
    >,
    sequenced
    <
      tag<FrameDataSetSequencedTag>
    >
  >
> FrameDataSetCollection;

typedef FrameDataSetCollection::index<FrameDataSetByIdTag>::type
FrameDataSetCollectionById;
typedef FrameDataSetCollection::index<FrameDataSetByOutputHashTag>::type
FrameDataSetCollectionByOutputHash;
typedef FrameDataSetCollection::index<FrameDataSetBySourceHashTag>::type
FrameDataSetCollectionBySourceHash;
typedef FrameDataSetCollection::index<FrameDataSetByFrameNameTag>::type
FrameDataSetCollectionByFrameName;
typedef FrameDataSetCollection::index<FrameDataSetSequencedTag>::type
FrameDataSetCollectionSequencedAccess;

class FrameDataSetSourceHashSearch
{
private:
  class FrameDataSetSourceHashSearchIterator
    : public std::iterator<std::forward_iterator_tag, FrameInfo>
  {
  private:
    size_t stage_;
    SourceHash source_hash_;
    const FrameDataSetCollectionBySourceHash & data_set_;
    FrameDataSetCollectionBySourceHash::const_iterator itr_;
    FrameDataSetCollectionBySourceHash::const_iterator begin_, current_end_;

  public:
    FrameDataSetSourceHashSearchIterator( const FrameDataSetCollectionBySourceHash & data_set,
      SourceHash source_hash, bool end );

    FrameDataSetSourceHashSearchIterator( const FrameDataSetSourceHashSearchIterator & it );

    FrameDataSetSourceHashSearchIterator & operator++();
    FrameDataSetSourceHashSearchIterator operator++( int );

    bool operator==( const FrameDataSetSourceHashSearchIterator & rhs ) const;
    bool operator!=( const FrameDataSetSourceHashSearchIterator & rhs ) const;

    const FrameInfo & operator*() const;
    const FrameInfo * operator->() const;
  };

  SourceHash source_hash_;
  const FrameDataSetCollectionBySourceHash & data_set_;
  FrameDataSetSourceHashSearchIterator begin_iterator_, end_iterator_;

public:
  typedef FrameDataSetSourceHashSearchIterator iterator;
  typedef FrameDataSetSourceHashSearchIterator const_iterator;

  FrameDataSetSourceHashSearch( const FrameDataSetCollectionBySourceHash & data_set, SourceHash source_hash );

  iterator begin() { return begin_iterator_; }
  iterator end() { return end_iterator_; }
};

class FrameDB : public BasicDatabase<FrameInfo, AlfalfaProtobufs::FrameInfo,
  FrameDataSetCollection, FrameDataSetSequencedTag>
{
private:
  size_t frame_id_;

public:
  FrameDB( const std::string & filename, const std::string & magic_number, OpenMode mode = OpenMode::READ )
    : BasicDatabase<FrameInfo, AlfalfaProtobufs::FrameInfo,
      FrameDataSetCollection, FrameDataSetSequencedTag>( filename, magic_number, mode ),
      frame_id_( 0 )
  {
    while (has_frame_id( frame_id_ ))
      frame_id_++;
  }

  std::pair<FrameDataSetCollectionByOutputHash::const_iterator, FrameDataSetCollectionByOutputHash::const_iterator>
  search_by_output_hash( const size_t output_hash ) const;

  std::pair<FrameDataSetSourceHashSearch::const_iterator, FrameDataSetSourceHashSearch::const_iterator>
  search_by_decoder_hash( const DecoderHash & decoder_hash ) const;

  size_t insert( FrameInfo frame );

  bool has_frame_id( const size_t frame_id ) const;
  bool has_frame_name( const FrameName & name ) const;
  const FrameInfo & search_by_frame_id( const size_t frame_id ) const;

  const FrameInfo & search_by_frame_name( const FrameName & name ) const;

};

#endif /* FRAME_DB */
