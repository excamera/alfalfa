#ifndef DB_HH
#define DB_HH

#include <map>
#include <cstring>
#include <fstream>
#include <iostream>
#include <boost/functional/hash.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>

#include "optional.hh"
#include "serialization.hh"

using namespace boost::multi_index;
using boost::multi_index_container;

enum class OpenMode
{
  READ,
  Create
};

class SerializableData
{
protected:
  std::string filename_;
  OpenMode mode_;

public:
  const std::string magic_number;

  SerializableData( const std::string & filename, const std::string & magic_number,
		    const OpenMode mode = OpenMode::READ )
    : filename_( filename ),
      mode_( mode ),
      magic_number( magic_number )
  {}
};

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag,
	 class MagicNumber>
class BasicDatabase : public SerializableData
{
protected:
  Collection collection_;
  static std::string magic_number() { return MagicNumber::magic; }

public:

  typedef typename Collection::template index<SequencedTag>::type SequencedAccess;

  size_t size() const;

  BasicDatabase( const std::string & filename, OpenMode mode = OpenMode::READ );
  void insert( RecordType record );

  typename SequencedAccess::iterator begin() { return this->collection_.get<SequencedTag>().begin(); }
  typename SequencedAccess::iterator begin() const { return this->collection_.get<SequencedTag>().begin(); }

  typename SequencedAccess::iterator end() { return this->collection_.get<SequencedTag>().end(); }
  typename SequencedAccess::iterator end() const { return this->collection_.get<SequencedTag>().end(); }

  void serialize( FileDescriptor && fd ) const;
  void deserialize();
};

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag,
	 class MagicNumber>
BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag, MagicNumber>
::BasicDatabase( const std::string & filename, OpenMode mode )
  : SerializableData( filename, magic_number(), mode ), collection_()
{
  if ( mode == OpenMode::READ ) {
    deserialize();
  }
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag,
	 class MagicNumber>
size_t BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag, MagicNumber>
  ::size() const
{
  return collection_.get<SequencedTag>().size();
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag,
	 class MagicNumber>
void BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag, MagicNumber>
  ::insert( RecordType record )
{
  this->collection_.insert( record );
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag,
	 class MagicNumber>
void BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag, MagicNumber>
  ::serialize( FileDescriptor && fd ) const
{
  ProtobufSerializer serializer( std::move( fd ) );

  // Writing the header
  serializer.write_string( magic_number() );

  // Writing entries
  for ( const RecordType & it : collection_.get<SequencedTag>() ) {
    serializer.write_protobuf( it.to_protobuf() );
  }
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag,
	 class MagicNumber>
void BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag, MagicNumber>
  ::deserialize()
{
  this->collection_.clear();

  ProtobufDeserializer deserializer( filename_ );

  // Reading the header
  if ( magic_number() != deserializer.read_string( magic_number().length() ) ) {
    throw std::runtime_error( "magic number mismatch: expecting " + magic_number() );
  }

  // Reading entries
  RecordProtobufType message;

  while ( deserializer.read_protobuf( message ) ) {
    insert( message );
  }
}

#endif
