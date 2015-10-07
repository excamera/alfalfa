#include <fstream>
#include <utility>

#include "database.hh"

using namespace std;

template<class KeyType, class ValueType>
Database<KeyType, ValueType>::Database( const std::string & filename )
  : filename_( filename ),
    hash_map_()
{
  std::ifstream fin( filename.c_str() );

  if ( fin.good() ) {

  }
}

template<class KeyType, class ValueType>
Optional<ValueType> Database<KeyType, ValueType>::get( const KeyType & key )
{
  auto entry_it = hash_map_.find( key );

  if ( entry_it != hash_map_.end() ) {
    return make_optional<ValueType>( true, entry_it->second );
  }
  else {
    return Optional<ValueType>();
  }
}

template<class KeyType, class ValueType>
bool Database<KeyType, ValueType>::insert( KeyType && key, ValueType && value )
{
  return hash_map_.emplace( make_pair( key, value ) ).second;
}

template<class KeyType, class ValueType>
bool Database<KeyType, ValueType>::update( KeyType && key, ValueType && value )
{
  auto entry_it = hash_map_.find( key );

  if ( entry_it == hash_map_.end() ) {
    return false;
  }

  entry_it->second = value;
  return true;
}

template<class KeyType, class ValueType>
void Database<KeyType, ValueType>::update_or_insert( KeyType && key, ValueType && value )
{
  hash_map_[key] = value;
}

template<class KeyType, class ValueType>
bool Database<KeyType, ValueType>::erase( KeyType & key )
{
  auto entry_it = hash_map_.find( key );

  if ( entry_it == hash_map_.end() ) {
    return false;
  }

  hash_map_.erase( entry_it );
  return true;
}

template<class KeyType, class ValueType>
bool Database<KeyType, ValueType>::has_key( const KeyType & key ) const
{
  return hash_map_.find( key ) != hash_map_.end();
}

template<class KeyType, class ValueType>
void Database<KeyType, ValueType>::save()
{
  /* TODO implement this */
}

template class Database<int, std::string>;
