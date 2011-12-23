/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/**
  \file    IOManager.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    August 2008
*/
#pragma once

#ifndef IOMANAGER_H
#define IOMANAGER_H

#include "StdTuvokDefines.h"
#ifdef _MSC_VER
# include <memory>
# include <tuple>
#else
# include <tr1/memory>
# include <tr1/tuple>
#endif
#include <algorithm>
#include <fstream>
#include <limits>
#include <list>
#include <string>
#include "Controller/MasterController.h"
#include "Basics/MC.h"
#include "Basics/SysTools.h"
#include "Basics/LargeRAWFile.h"
#include "Basics/Mesh.h"
#include "Basics/TuvokException.h"
#include "TransferFunction1D.h"
#include "AbstrGeoConverter.h"

typedef std::tr1::tuple<std::string,std::string,bool,bool> tConverterFormat;

#define DEFAULT_BRICKSIZE (256)
#define DEFAULT_BRICKOVERLAP (4)
#define DEFAULT_INCORESIZE (DEFAULT_BRICKSIZE*DEFAULT_BRICKSIZE*DEFAULT_BRICKSIZE)

class AbstrConverter;
class FileStackInfo;
class RangeInfo;
class UVF;
class GeometryDataBlock;

namespace tuvok {
  class AbstrRenderer;
  class Dataset;
  class FileBackedDataset;
  class UVFDataset;
  class MasterController;
  namespace io {
    class DSFactory;
  }
}

class MergeDataset {
public:
  MergeDataset(std::string _strFilename="", uint64_t _iHeaderSkip=0, bool _bDelete=false,
               double _fScale=1.0, double _fBias=0.0) :
    strFilename(_strFilename),
    iHeaderSkip(_iHeaderSkip),
    bDelete(_bDelete),
    fScale(_fScale),
    fBias(_fBias)
  {}

  std::string strFilename;
  uint64_t iHeaderSkip;
  bool bDelete;
  double fScale;
  double fBias;
};

template <class T> class DataMerger {
public:
  DataMerger(const std::vector<MergeDataset>& strFiles,
             const std::string& strTarget, uint64_t iElemCount,
             tuvok::MasterController* pMasterController,
             bool bUseMaxMode) :
    bIsOK(false)
  {
    AbstrDebugOut& dbg = *(pMasterController->DebugOut());
    dbg.Message(_func_,"Copying first file %s ...",
                SysTools::GetFilename(strFiles[0].strFilename).c_str());
    if (!LargeRAWFile::Copy(strFiles[0].strFilename, strTarget,
                            strFiles[0].iHeaderSkip)) {
      dbg.Error("Could not copy '%s' to '%s'", strFiles[0].strFilename.c_str(),
                strTarget.c_str());
      bIsOK = false;
      return;
    }

    dbg.Message(_func_,"Merging ...");
    LargeRAWFile target(strTarget);
    target.Open(true);

    if (!target.IsOpen()) {
      dbg.Error("Could not open '%s'", strTarget.c_str());
      remove(strTarget.c_str());
      bIsOK = false;
      return;
    }

    uint64_t iCopySize = std::min(iElemCount,BLOCK_COPY_SIZE/2)/sizeof(T);
    T* pTargetBuffer = new T[size_t(iCopySize)];
    T* pSourceBuffer = new T[size_t(iCopySize)];
    for (size_t i = 1;i<strFiles.size();i++) {
      dbg.Message(_func_,"Merging with file %s ...",
                  SysTools::GetFilename(strFiles[i].strFilename).c_str());
      LargeRAWFile source(strFiles[i].strFilename, strFiles[i].iHeaderSkip);
      source.Open(false);
      if (!source.IsOpen()) {
        dbg.Error(_func_, "Could not open '%s'!",
                  strFiles[i].strFilename.c_str());
        delete [] pTargetBuffer;
        delete [] pSourceBuffer;
        target.Close();
        remove(strTarget.c_str());
        bIsOK = false;
        return;
      }

      uint64_t iReadSize=0;
      do {
         source.ReadRAW((unsigned char*)pSourceBuffer, iCopySize*sizeof(T));
         iCopySize = target.ReadRAW((unsigned char*)pTargetBuffer,
                                    iCopySize*sizeof(T))/sizeof(T);

         if (bUseMaxMode) {
           if (i == 1) {
             for (uint64_t j = 0;j<iCopySize;j++) {
               pTargetBuffer[j] = std::max<T>(
                 T(std::min<double>(
                   strFiles[0].fScale*(pTargetBuffer[j] + strFiles[0].fBias),
                   static_cast<double>(std::numeric_limits<T>::max())
                 )),
                 T(std::min<double>(
                   strFiles[i].fScale*(pSourceBuffer[j] + strFiles[i].fBias),
                   static_cast<double>(std::numeric_limits<T>::max()))
                 )
               );
             }
           } else {
             for (uint64_t j = 0;j<iCopySize;j++) {
               pTargetBuffer[j] = std::max<T>(
                 pTargetBuffer[j],
                 T(std::min<double>(strFiles[i].fScale*(pSourceBuffer[j] +
                                                        strFiles[i].fBias),
                                    static_cast<double>(std::numeric_limits<T>::max())))
               );
             }
           }
         } else {
           if (i == 1) {
             for (uint64_t j = 0;j<iCopySize;j++) {
               T a = T(std::min<double>(
                 strFiles[0].fScale*(pTargetBuffer[j] + strFiles[0].fBias),
                 static_cast<double>(std::numeric_limits<T>::max())
               ));
               T b = T(std::min<double>(
                 strFiles[i].fScale*(pSourceBuffer[j] + strFiles[i].fBias),
                 static_cast<double>(std::numeric_limits<T>::max())
               ));
               T val = a + b;

               if (val < a || val < b) // overflow
                 pTargetBuffer[j] = std::numeric_limits<T>::max();
               else
                 pTargetBuffer[j] = val;
             }
           } else {
             for (uint64_t j = 0;j<iCopySize;j++) {
               T b = T(std::min<double>(
                 strFiles[i].fScale*(pSourceBuffer[j] + strFiles[i].fBias),
                 static_cast<double>(std::numeric_limits<T>::max())
               ));
               T val = pTargetBuffer[j] + b;

               if (val < pTargetBuffer[j] || val < b) // overflow
                 pTargetBuffer[j] = std::numeric_limits<T>::max();
               else
                 pTargetBuffer[j] = val;
             }
           }
         }
         target.SeekPos(iReadSize*sizeof(T));
         target.WriteRAW((unsigned char*)pTargetBuffer, iCopySize*sizeof(T));
         iReadSize += iCopySize;
      } while (iReadSize < iElemCount);
      source.Close();
    }

    delete [] pTargetBuffer;
    delete [] pSourceBuffer;
    target.Close();

    bIsOK = true;
  }

  bool IsOK() const {return bIsOK;}

private:
  bool bIsOK;
};


class MCData {
public:
  MCData(const std::string& strTargetFile) :
    m_strTargetFile(strTargetFile)
  {}

  virtual ~MCData() {}

  virtual bool PerformMC(LargeRAWFile_ptr pSourceFile, const std::vector<uint64_t> vBrickSize, const std::vector<uint64_t> vBrickOffset) = 0;

protected:
  std::string m_strTargetFile;
};


template <class T> class MCDataTemplate  : public MCData {
public:
  MCDataTemplate(const std::string& strTargetFile, T TIsoValue, 
                 const FLOATVECTOR3& vScale, 
                 UINT64VECTOR3 vDataSize, tuvok::AbstrGeoConverter* conv,
                 const FLOATVECTOR4& vColor) :
    MCData(strTargetFile),
    m_TIsoValue(TIsoValue),
    m_pData(NULL),
    m_iIndexoffset(0),
    m_pMarchingCubes(new MarchingCubes<T>()),
    m_vDataSize(vDataSize),
    m_conv(conv),
    m_vColor(vColor),
    m_vScale(vScale)
  {
  }

  virtual ~MCDataTemplate() {
    delete m_pMarchingCubes;
    delete m_pData;
    tuvok::Mesh m = tuvok::Mesh(m_vertices, m_normals, tuvok::TexCoordVec(),
                                tuvok::ColorVec(), m_indices, m_indices, 
                                tuvok::IndexVec(),tuvok::IndexVec(), 
                                false,false,"Marching Cubes mesh by ImageVis3D",
                                tuvok::Mesh::MT_TRIANGLES);
    m.SetDefaultColor(m_vColor);
    m_conv->ConvertToNative(m, m_strTargetFile);
  }

  virtual bool PerformMC(LargeRAWFile_ptr pSourceFile, const std::vector<uint64_t> vBrickSize, const std::vector<uint64_t> vBrickOffset) {

    uint64_t uSize = 1;
    for (size_t i = 0;i<vBrickSize.size();i++) uSize *= vBrickSize[i];
    // Can't use bricks that we can't store in a single array.
    // Really, the whole reason we're bricking is to prevent larger-than-core
    // data, so this should never happen anyway -- we'd have no way to create
    // such a brick.
    assert(uSize <= std::numeric_limits<size_t>::max());

    size_t iSize = static_cast<size_t>(
                     std::min<uint64_t>(uSize, std::numeric_limits<size_t>::max())
                   );
    if (!m_pData) {   // since we know that no brick is larger than the first we can create a fixed array on first invocation
      m_pData = new T[iSize];
    }

    pSourceFile->SeekStart();
    pSourceFile->ReadRAW((unsigned char*)m_pData, size_t(iSize*sizeof(T)));

    // extract isosurface
    m_pMarchingCubes->SetVolume(int(vBrickSize[0]), int(vBrickSize[1]), int(vBrickSize[2]), m_pData);
    m_pMarchingCubes->Process(m_TIsoValue);

    // brick scale
    float fMaxSize = (FLOATVECTOR3(m_vDataSize) * m_vScale).maxVal();

    FLOATVECTOR3 vecBrickOffset(vBrickOffset);
    vecBrickOffset = vecBrickOffset * m_vScale;

    for (int i = 0;i<m_pMarchingCubes->m_Isosurface->iVertices;i++) {
        m_vertices.push_back((m_pMarchingCubes->m_Isosurface->vfVertices[i]+vecBrickOffset-FLOATVECTOR3(m_vDataSize)/2.0f)/fMaxSize);
    }

    for (int i = 0;i<m_pMarchingCubes->m_Isosurface->iVertices;i++) {
      m_normals.push_back(m_pMarchingCubes->m_Isosurface->vfNormals[i]);
    }    

    for (int i = 0;i<m_pMarchingCubes->m_Isosurface->iTriangles;i++) {
      m_indices.push_back(m_pMarchingCubes->m_Isosurface->viTriangles[i].x+m_iIndexoffset);
      m_indices.push_back(m_pMarchingCubes->m_Isosurface->viTriangles[i].y+m_iIndexoffset);
      m_indices.push_back(m_pMarchingCubes->m_Isosurface->viTriangles[i].z+m_iIndexoffset);
    }

    m_iIndexoffset += m_pMarchingCubes->m_Isosurface->iVertices;

    return true;
  }

protected:
  T                  m_TIsoValue;
  T*                 m_pData;
  uint32_t             m_iIndexoffset;
  MarchingCubes<T>*  m_pMarchingCubes;
  UINT64VECTOR3      m_vDataSize;
  tuvok::AbstrGeoConverter* m_conv;
  FLOATVECTOR4       m_vColor;
  FLOATVECTOR3       m_vScale;
  tuvok::VertVec     m_vertices;
  tuvok::NormVec     m_normals;
  tuvok::IndexVec    m_indices;
};

class IOManager {
public:
  IOManager();
  ~IOManager();

  std::vector<std::tr1::shared_ptr<FileStackInfo> >
    ScanDirectory(std::string strDirectory) const;
  bool ConvertDataset(FileStackInfo* pStack,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool ConvertDataset(const std::string& strFilename,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bNoUserInteraction,
                      uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool ConvertDataset(const std::list<std::string>& files,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bNoUserInteraction,
                      uint64_t iMaxBrickSize,
                      uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;
  bool MergeDatasets(const std::vector <std::string>& strFilenames,
                     const std::vector <double>& vScales,
                     const std::vector<double>& vBiases,
                     const std::string& strTargetFilename,
                     const std::string& strTempDir,
                     bool bUseMaxMode=true,
                     bool bNoUserInteraction=false) const;
  tuvok::UVFDataset* ConvertDataset(FileStackInfo* pStack,
                                    const std::string& strTargetFilename,
                                    const std::string& strTempDir,
                                    tuvok::AbstrRenderer* requester,
                                    uint64_t iMaxBrickSize,
                                    uint64_t iBrickOverlap,
                                    bool bQuantizeTo8Bit=false) const;
  tuvok::UVFDataset* ConvertDataset(const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    const std::string& strTempDir,
                                    tuvok::AbstrRenderer* requester,
                                    uint64_t iMaxBrickSize,
                                    uint64_t iBrickOverlap,
                                    bool bQuantizeTo8Bit=false) const;

  /// evaluates the given expression. v[n] in the expression refers to
  /// the volume given by volumes[n].
  void EvaluateExpression(const char* expr,
                          const std::vector<std::string> volumes,
                          const std::string& out_fn) const
                          throw(tuvok::Exception);


  bool ReBrickDataset(const std::string& strSourceFilename,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const uint64_t iMaxBrickSize,
                      const uint64_t iBrickOverlap,
                      bool bQuantizeTo8Bit=false) const;

  // conveniance calls that use the default bricksizes and overlaps
  tuvok::UVFDataset* ConvertDataset(FileStackInfo* pStack,
                                    const std::string& strTargetFilename,
                                    const std::string& strTempDir,
                                    tuvok::AbstrRenderer* requester,
                                    const bool bQuantizeTo8Bit=false) const {
    return ConvertDataset(pStack,strTargetFilename,strTempDir,requester,m_iMaxBrickSize, m_iBrickOverlap, bQuantizeTo8Bit);
  }
  tuvok::UVFDataset* ConvertDataset(const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    const std::string& strTempDir,
                                    tuvok::AbstrRenderer* requester,
                                    const bool bQuantizeTo8Bit=false) {
    return ConvertDataset(strFilename,strTargetFilename,strTempDir,requester,m_iMaxBrickSize, m_iBrickOverlap,bQuantizeTo8Bit);
  }
  bool ConvertDataset(FileStackInfo* pStack,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bQuantizeTo8Bit=false) const{
    return ConvertDataset(pStack,strTargetFilename,strTempDir,m_iMaxBrickSize, m_iBrickOverlap, bQuantizeTo8Bit);
  }
  bool ConvertDataset(const std::string& strFilename,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bNoUserInteraction=false,
                      const bool bQuantizeTo8Bit=false) const {
    return ConvertDataset(strFilename,strTargetFilename,strTempDir,bNoUserInteraction,m_iMaxBrickSize,m_iBrickOverlap, bQuantizeTo8Bit);
  }
  bool ConvertDataset(const std::list<std::string>& files,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      const bool bNoUserInteraction=false,
                      const bool bQuantizeTo8Bit=false) const {
    return ConvertDataset(files,strTargetFilename,strTempDir,bNoUserInteraction,m_iMaxBrickSize,m_iBrickOverlap, bQuantizeTo8Bit);
  }
  bool ReBrickDataset(const std::string& strSourceFilename,
                      const std::string& strTargetFilename,
                      const std::string& strTempDir,
                      bool bQuantizeTo8Bit=false) const {
    return ReBrickDataset(strSourceFilename,strTargetFilename,strTempDir,m_iMaxBrickSize,m_iBrickOverlap,bQuantizeTo8Bit);
  }

  tuvok::Mesh* LoadMesh(const std::string& meshfile) const;

  void AddMesh(const UVF* sourceDataset,
                  const std::string& trisoup_file,
                  const std::string& uvf) const;

  tuvok::Dataset* LoadDataset(const std::string& strFilename,
                              tuvok::AbstrRenderer* requester) const;
  tuvok::Dataset* CreateDataset(const std::string& filename,
                                uint64_t max_brick_size, bool verify) const;
  void AddReader(std::tr1::shared_ptr<tuvok::FileBackedDataset>);
  bool AnalyzeDataset(const std::string& strFilename, RangeInfo& info,
                      const std::string& strTempDir) const;
  bool NeedsConversion(const std::string& strFilename) const;
  bool Verify(const std::string& strFilename) const;

  bool ExportMesh(const tuvok::Mesh* mesh, const std::string& strTargetFilename);
  bool ExportDataset(const tuvok::UVFDataset* pSourceData, uint64_t iLODlevel,
                     const std::string& strTargetFilename,
                     const std::string& strTempDir) const;
  bool ExtractIsosurface(const tuvok::UVFDataset* pSourceData,
                         uint64_t iLODlevel, double fIsovalue,
                         const FLOATVECTOR4& vfColor,
                         const std::string& strTargetFilename,
                         const std::string& strTempDir) const;
  bool ExtractImageStack(const tuvok::UVFDataset* pSourceData,
                         const TransferFunction1D* pTrans,
                         uint64_t iLODlevel, 
                         const std::string& strTargetFilename,
                         const std::string& strTempDir,
                         bool bAllDirs) const;


  void RegisterExternalConverter(AbstrConverter* pConverter);
  void RegisterFinalConverter(AbstrConverter* pConverter);

  std::string GetLoadDialogString() const;
  std::string GetExportDialogString() const;
  std::string GetImageExportDialogString() const;


  std::vector< std::pair <std::string, std::string > >
    GetImportFormatList() const;
  std::vector< std::pair <std::string, std::string > >
    GetExportFormatList() const;
  std::vector< tConverterFormat > GetFormatList() const;
  AbstrConverter* GetConverterForExt(std::string ext,
                                     bool bMustSupportExport,
                                     bool bMustSupportImport) const;

  std::string GetLoadGeoDialogString() const;
  std::string GetGeoExportDialogString() const;
  std::vector< std::pair <std::string, std::string > >
    GetGeoImportFormatList() const;
  std::vector< std::pair <std::string, std::string > >
    GetGeoExportFormatList() const;
  std::vector< tConverterFormat > GetGeoFormatList() const;
  tuvok::AbstrGeoConverter* GetGeoConverterForExt(std::string ext,
                                                  bool bMustSupportExport,
                                                  bool bMustSupportImport) const;


  uint64_t GetMaxBrickSize() const {return m_iMaxBrickSize;}
  uint64_t GetBrickOverlap() const {return m_iBrickOverlap;}
  uint64_t GetIncoresize() const {return m_iIncoresize;}

  bool SetMaxBrickSize(const uint64_t iMaxBrickSize);
  bool SetBrickOverlap(const uint64_t iBrickOverlap);

private:
  std::vector<tuvok::AbstrGeoConverter*> m_vpGeoConverters;
  std::vector<AbstrConverter*>    m_vpConverters;
  AbstrConverter*                 m_pFinalConverter;
  std::auto_ptr<tuvok::io::DSFactory> m_dsFactory;

  uint64_t m_iMaxBrickSize;
  uint64_t m_iBrickOverlap;
  uint64_t m_iIncoresize;

  void CopyToTSB(const tuvok::Mesh* m, GeometryDataBlock* tsb) const;
};

#endif // IOMANAGER_H
