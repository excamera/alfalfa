/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef TEMP_FILE_HH
#define TEMP_FILE_HH

#include <string>
#include <vector>

#include "file_descriptor.hh"

class UniqueFile
{
private:
  std::vector<char> mutable_temp_filename_;
  FileDescriptor fd_;

protected:
  bool moved_away_;

public:
  UniqueFile( const std::string & filename_template );
  virtual ~UniqueFile() {}

  std::string name( void ) const;

  void write( const std::string & contents );

  FileDescriptor & fd( void ) { return fd_; }
  const FileDescriptor & fd( void ) const { return fd_; }

  /* ban copying */
  UniqueFile( const UniqueFile & other ) = delete;
  UniqueFile & operator=( const UniqueFile & other ) = delete;

  /* allow move constructor */
  UniqueFile( UniqueFile && other );

  /* ... but not move assignment operator */
  UniqueFile & operator=( UniqueFile && other ) = delete;
};

/* TempFile is deleted when object destroyed */
class TempFile : public UniqueFile
{
public:
  using UniqueFile::UniqueFile;

  /* allow move constructor */
  TempFile( TempFile && other ) : UniqueFile( std::move( other ) ) {}

  ~TempFile();
};

#endif
