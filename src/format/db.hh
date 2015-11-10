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

#define MAX_MAGIC_NUMBER_LENGTH 20

enum class OpenMode
{
  READ,
  APPEND,
  TRUNCATE
};

class SerializableData
{
protected:
  std::string filename_;
  OpenMode mode_;
  bool good_;

public:
  const std::string magic_number;

  SerializableData( const std::string & filename, const std::string & magic_number,
    OpenMode mode = OpenMode::READ );

  bool good() const { return good_; }
};

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag>
class BasicDatabase : public SerializableData
{
protected:
  Collection collection_;

public:

  typedef typename Collection::template index<SequencedTag>::type SequencedAccess;

  size_t size() const;

  BasicDatabase( const std::string & filename, const std::string & magic_number,
    OpenMode mode = OpenMode::READ );
  void insert( RecordType record );
  void merge( const BasicDatabase & db );

  typename SequencedAccess::iterator begin() { return this->collection_.get<SequencedTag>().begin(); }
  typename SequencedAccess::iterator end() { return this->collection_.get<SequencedTag>().end(); }

  bool serialize( const std::string & filename = "" );
  bool deserialize( const std::string & filename = "" );
};

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag>
BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag>
  ::BasicDatabase( const std::string & filename, const std::string & magic_number,
  OpenMode mode )
  : SerializableData( filename, magic_number, mode ), collection_()
{
  if ( good_ == true and ( mode == OpenMode::READ or mode == OpenMode::APPEND ) ) {
    good_ = deserialize();
  }
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag>
size_t BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag>
  ::size() const
{
  return collection_.get<SequencedTag>().size();
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag>
void BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag>
  ::insert( RecordType record )
{
  this->collection_.insert( record );
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag>
void BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag>
  ::merge( const BasicDatabase & db )
{
  for ( auto const & item : db.collection_.get<SequencedTag>() ) {
    insert( item );
  }
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag>
bool BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag>
  ::serialize( const std::string & filename )
{
  if ( mode_ == OpenMode::READ ) {
    return false;
  }

  std::string target_filename = ( filename.length() == 0 ) ? filename_ : filename;
  filename_ = target_filename;
  ProtobufSerializer serializer( filename_ );

  // Writing the header
  if ( not serializer.write_raw( magic_number.c_str(), magic_number.length() ) ) {
    return false;
  }

  // Writing entries
  for ( const RecordType & it : collection_.get<SequencedTag>() ) {
    RecordProtobufType message;
    to_protobuf( it, message );

    if ( not serializer.write_protobuf( message ) ) {
      return false;
    }
  }

  return true;
}

template<class RecordType, class RecordProtobufType, class Collection, class SequencedTag>
bool BasicDatabase<RecordType, RecordProtobufType, Collection, SequencedTag>
  ::deserialize( const std::string & filename )
{
  this->collection_.clear();

  std::string target_filename = ( filename.length() == 0 ) ? filename_ : filename;
  ProtobufDeserializer deserializer( target_filename );

  // Reading the header
  char target_magic_number[ MAX_MAGIC_NUMBER_LENGTH ] = {0};
  deserializer.read_raw( target_magic_number, magic_number.length() );

  if ( magic_number != target_magic_number ) {
    return false;
  }

  // Reading entries
  RecordProtobufType message;

  while ( deserializer.read_protobuf( message ) ) {
    RecordType record;
    from_protobuf( message, record );
    insert( record );
  }

  return true;
}

#endif
