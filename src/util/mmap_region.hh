/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef MMAP_REGION_HH
#define MMAP_REGION_HH

#include <cstdint>

class MMap_Region
{
private:
  uint8_t *addr_;
  size_t length_;

public:
  MMap_Region( const size_t length, const int prot, const int flags, const int fd );

  ~MMap_Region();

  /* Disallow copying */
  MMap_Region( const MMap_Region & other ) = delete;
  MMap_Region & operator=( const MMap_Region & other ) = delete;

  /* Allow moving */
  MMap_Region( MMap_Region && other );

  /* Getter */
  uint8_t *addr() const { return addr_; }
};

#endif /* MMAP_REGION_HH */
