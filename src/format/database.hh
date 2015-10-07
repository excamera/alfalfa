#ifndef DATABASE_HH
#define DATABASE_HH

#include <string>
#include <unordered_map>

#include "optional.hh"

using namespace std;

template<class KeyType, class ValueType>
class Database
{
private:
  std::string filename_;
  std::unordered_map<KeyType, ValueType> hash_map_;

public:
  Database( const std::string & filename );

  Optional<ValueType> get( const KeyType & key );
  bool insert( KeyType && key, ValueType && value );
  bool update( KeyType && key, ValueType && value );
  void update_or_insert( KeyType && key, ValueType && value );
  bool erase( KeyType & key );
  bool has_key( const KeyType & key ) const;

  void save();
};

#endif /* DATABASE_HH */
