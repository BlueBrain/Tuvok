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
  \file    QVISConverter.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    December 2008
*/

#include "QVISConverter.h"
#include "IOManager.h"  // for the size defines
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>
#include <IO/KeyValueFileParser.h>
#include <fstream>

using namespace std;


QVISConverter::QVISConverter()
{
  m_vConverterDesc = "QVis Data";
  m_vSupportedExt.push_back("DAT");
}

bool QVISConverter::ConvertToRAW(const std::string& strSourceFilename, 
                            const std::string&, MasterController* pMasterController, bool,
                            UINT64& iHeaderSkip, UINT64& iComponentSize, UINT64& iComponentCount, 
                            bool& bConvertEndianess, bool& bSigned, UINTVECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::string& strTitle, std::string& strSource, 
                            UVFTables::ElementSemanticTable& eType, std::string& strIntermediateFile,
                            bool& bDeleteIntermediateFile) {

  pMasterController->DebugOut()->Message("QVISConverter::ConvertToRAW","Attempting to convert QVIS dataset %s", strSourceFilename.c_str());

  bDeleteIntermediateFile = false;
  eType             = UVFTables::ES_UNDEFINED;
  strTitle          = "Qvis data";
  strSource         = SysTools::GetFilename(strSourceFilename);
  iHeaderSkip       = 0; 
  iComponentSize    = 8;
  iComponentCount   = 1;
  bSigned           = false;
  /// \todo  detect big endian DAT/RAW combinations and set the conversion 
  ///        parameter accordingly instead of always assuming it is little 
  ///        endian and thius converting if the machine is big endian 
  bConvertEndianess = EndianConvert::IsBigEndian();
  string strRAWFile;

  KeyValueFileParser parser(strSourceFilename);

  if (parser.FileReadable())  {
    KeyValPair* format = parser.GetData("FORMAT");
    if (format == NULL)
      return false;
    else {
      if (format->strValueUpper == "CHAR") {
        bSigned = false;
        iComponentSize = 8;
        iComponentCount = 1;
      } else if (format->strValueUpper == "UCHAR" || format->strValueUpper == "BYTE") {
        bSigned = false;
        iComponentSize = 8;
        iComponentCount = 1;
      } else if (format->strValueUpper == "SHORT") {
        bSigned = true;
        iComponentSize = 16;
        iComponentCount = 1;
      } else if (format->strValueUpper == "USHORT") {
        bSigned = false;
        iComponentSize = 16;
        iComponentCount = 1;
      } else if (format->strValueUpper == "UCHAR4") {
        bSigned = false;
        iComponentSize = 8;
        iComponentCount = 4;
      } else if (format->strValueUpper == "FLOAT") {
        bSigned = true;
        iComponentSize = 32;
        iComponentCount = 1;
      }
    }

    KeyValPair* objectfilename = parser.GetData("OBJECTFILENAME");
    if (objectfilename == NULL) {
      pMasterController->DebugOut()->Warning("QVISConverter::ConvertToRAW","This is not a valid QVIS dat file.");
      return false; 
    } else
        strIntermediateFile = objectfilename->strValue;

    KeyValPair* resolution = parser.GetData("RESOLUTION");
    if (resolution == NULL || resolution->vuiValue.size() != 3) {
      pMasterController->DebugOut()->Warning("QVISConverter::ConvertToRAW","This is not a valid QVIS dat file.");
      return false; 
    } else 
      vVolumeSize = UINTVECTOR3(resolution->vuiValue);

    KeyValPair* sliceThickness = parser.GetData("SLICETHICKNESS");
    if (sliceThickness == NULL || sliceThickness->vuiValue.size() != 3) {
      pMasterController->DebugOut()->Warning("QVISConverter::ConvertToRAW","This is not a valid QVIS dat file.");
      vVolumeAspect = FLOATVECTOR3(1,1,1);
    } else {
      vVolumeAspect = FLOATVECTOR3(sliceThickness->vfValue);
      vVolumeAspect = vVolumeAspect / vVolumeAspect.maxVal();
    }

    strRAWFile = SysTools::GetPath(strSourceFilename) + strRAWFile;
  } else return false;

  return true;
}

bool QVISConverter::ConvertToNative(const std::string& strRawFilename, const std::string& strTargetFilename, UINT64 iHeaderSkip,
                             UINT64 iComponentSize, UINT64 iComponentCount, bool bSigned, bool bFloatingPoint,
                             UINTVECTOR3 vVolumeSize,FLOATVECTOR3 vVolumeAspect, MasterController* pMasterController, bool bNoUserInteraction) {

  // compute fromat string
  string strFormat;

  if (bFloatingPoint && bSigned && iComponentSize == 32 && iComponentCount == 1)
    strFormat = "FLOAT";
  else
  if (!bFloatingPoint && bSigned && iComponentSize == 8 && iComponentCount == 1) 
    strFormat = "CHAR";
  else
  if (!bFloatingPoint && !bSigned && iComponentSize == 8 && iComponentCount == 1) 
    strFormat = "UCHAR";
  else
  if (!bFloatingPoint && bSigned && iComponentSize == 16 && iComponentCount == 1) 
    strFormat = "SHORT";
  else
  if (!bFloatingPoint && !bSigned && iComponentSize == 16 && iComponentCount == 1) 
    strFormat = "USHORT";
  else
  if (!bFloatingPoint && !bSigned && iComponentSize == 8 && iComponentCount == 4) 
    strFormat = "UCHAR4";
  else {
    pMasterController->DebugOut()->Error("QVISConverter::ConvertToNative","This data type is not supported by QVIS DAT/RAW files.");
    return false;
  }                               
                               
  // create DAT textfile from metadata
  string strTargetRAWFilename = strTargetFilename+".raw";

  ofstream fTarget(strTargetFilename.c_str());  
  if (!fTarget.is_open()) {
    pMasterController->DebugOut()->Error("QVISConverter::ConvertToNative","Unable to open target file %s.", strTargetFilename.c_str());
    return false;
  }

  fTarget << "ObjectFileName: " << strTargetRAWFilename << endl;
  fTarget << "TaggedFileName: ---" << endl;
  fTarget << "Resolution:     " << vVolumeSize.x << " " << vVolumeSize.y << " "<< vVolumeSize.z << endl;
  fTarget << "SliceThickness: " << vVolumeAspect.x << " " << vVolumeAspect.y << " "<< vVolumeAspect.z << endl;
  fTarget << "Format:         " << strFormat << endl;
  fTarget << "ObjectType:     TEXTURE_VOLUME_OBJECT" << endl;
  fTarget << "ObjectModel:    RGBA" << endl;
  fTarget << "GridType:       EQUIDISTANT" << endl;
  fTarget.close();

  // copy RAW file using the parent's call
  bool bRAWSuccess = RAWConverter::ConvertToNative(strRawFilename, strTargetRAWFilename, iHeaderSkip,
                                                   iComponentSize, iComponentCount, bSigned, bFloatingPoint,
                                                   vVolumeSize, vVolumeAspect, pMasterController, bNoUserInteraction);

  if (bRAWSuccess) {
    return true;
  } else {
    pMasterController->DebugOut()->Error("QVISConverter::ConvertToNative","Error creating raw target file %s.", strTargetRAWFilename.c_str());
    remove(strTargetFilename.c_str());
    return false;
  }
}
