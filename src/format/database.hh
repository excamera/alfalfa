#ifndef DATABASE_HH
#define DATABASE_HH

#include <string>
#include <sparsehash/sparse_hash_map>
#include <sparsehash/dense_hash_map>

using namespace std;

template<class KeyType, class ValueType>
class Database
{
private:
  std::string filename_;
  google::dense_hash_map<KeyType, ValueType, std::hash<KeyType>> hash_map_;

public:
  Database( const std::string & filename );

  ValueType get( const KeyType & key );
  void insert( KeyType & key, ValueType & value );
  void update( KeyType & key, ValueType & value );
  void insert_or_update( KeyType & key, ValueType & value );
  bool has_key( const KeyType & key ) const;

  void save();
};

#endif /* DATABASE_HH */
