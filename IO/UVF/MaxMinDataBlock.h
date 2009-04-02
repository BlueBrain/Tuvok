#pragma once

#ifndef MAXMINDATABLOCK_H
#define MAXMINDATABLOCK_H

#include "RasterDataBlock.h"
#include "../../Basics/Vectors.h"

template<class T, class S> class MaxMinElemen {
public:
  MaxMinElemen() :
   minScalar(0),
   maxScalar(0),
   minGradient(0),
   maxGradient(0)
  {}

  MaxMinElemen( T _minScalar, T _maxScalar, S _minGradient, S _maxGradient) :
   minScalar(_minScalar),
   maxScalar(_maxScalar),
   minGradient(_minGradient),
   maxGradient(_maxGradient)
  {}

  void Merge(const MaxMinElemen<T,S>& other) {
    minScalar = (minScalar > other.minScalar) ? other.minScalar : minScalar;
    maxScalar = (maxScalar < other.maxScalar) ? other.maxScalar : maxScalar;
    minGradient = (minGradient > other.minGradient) ? other.minGradient : minGradient;
    maxGradient = (maxGradient < other.maxGradient) ? other.maxGradient : maxGradient;
  }

  T minScalar;
  T maxScalar;
  S minGradient;
  S maxGradient;
};

typedef MaxMinElemen<double, double> InternalMaxMinElement;

class MaxMinDataBlock : public DataBlock
{
public:
  MaxMinDataBlock(UINT64 iComponentCount);
  ~MaxMinDataBlock();
  MaxMinDataBlock(const MaxMinDataBlock &other);
  MaxMinDataBlock(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian);

  virtual MaxMinDataBlock& operator=(const MaxMinDataBlock& other);
  virtual UINT64 ComputeDataSize() const;

  const InternalMaxMinElement& GetValue(size_t iIndex, UINT64 iComponent=0);
  void StartNewValue();
  void MergeData(const std::vector<DOUBLEVECTOR4>& fMaxMinData);

protected:
  std::vector< std::vector<InternalMaxMinElement> > m_vfMaxMinData;
  size_t                                            m_iComponentCount;

  virtual UINT64 GetHeaderFromFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian);
  virtual UINT64 CopyToFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual UINT64 GetOffsetToNextBlock() const;

  virtual DataBlock* Clone();
  void MergeData(const InternalMaxMinElement& data, const UINT64 iComponent);
};

#endif // MAXMINDATABLOCK_H
