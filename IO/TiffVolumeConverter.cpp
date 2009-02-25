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
  \file    TiffVolumeConverter.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#include "../3rdParty/tiff/tiffio.h"
#include "TiffVolumeConverter.h"
#include "../StdTuvokDefines.h"
#include "../Controller/MasterController.h"
#include "../Basics/SysTools.h"

#ifdef __GNUC__
# define _malloc __attribute__((malloc))
#else
# define _malloc /* nothing */
#endif

static void tv_dimensions(TIFF *, size_t dims[3]);
_malloc static BYTE* tv_read_slice(TIFF *, AbstrDebugOut &);
#if 0
// currently disabled -- mistakenly thought I was reading 4-component data for
// a while.  left in because we'll probably want to be able to convert
// 4-component data to 1-component data at some point.
_malloc static UINT32* tv_vector_to_scalar_magnitude(UINT32 *, size_t, size_t);
#endif

TiffVolumeConverter::TiffVolumeConverter()
{
  m_vConverterDesc = "TIFF Volume (Image stack)";
  m_vSupportedExt.push_back("OME.TIF");
  m_vSupportedExt.push_back("OME.TIFF");
  m_vSupportedExt.push_back("TIF");
  m_vSupportedExt.push_back("TIFF");
}

bool
TiffVolumeConverter::ConvertToRAW(const std::string& strSourceFilename,
                                  const std::string& strTempDir,
                                  MasterController* pMasterController,
                                  bool, UINT64& iHeaderSkip,
                                  UINT64& iComponentSize,
                                  UINT64& iComponentCount,
                                  bool& bConvertEndianess, bool& bSigned,
                                  bool& bIsFloat, UINTVECTOR3& vVolumeSize,
                                  FLOATVECTOR3& vVolumeAspect,
                                  std::string& strTitle,
                                  std::string& strSource,
                                  UVFTables::ElementSemanticTable& eType,
                                  std::string& strIntermediateFile,
                                  bool& bDeleteIntermediateFile)
{
  AbstrDebugOut& dbg = *(pMasterController->DebugOut());
  dbg.Message(_func_, "Attempting to convert TiffVolume: %s",
              strSourceFilename.c_str());

  TIFF *tif = TIFFOpen(strSourceFilename.c_str(), "r");
  if(tif == NULL) {
    dbg.Error(_func_, "Could not open %s", strSourceFilename.c_str());
    return false;
  }

  // Get the dimensions of the volume.
  {
    size_t dims[3];
    tv_dimensions(tif, dims);
    vVolumeSize[0] = dims[0];
    vVolumeSize[1] = dims[1];
    vVolumeSize[2] = dims[2];
    dbg.Message(_func_, "TiffVolume dimensions: %zux%zux%zu",
                dims[0], dims[1], dims[2]);
    if(dims[2] <= 1) {
      dbg.Error(_func_, "TIFF is not a volume; use "
                        "`Load Dataset from Directory' instead!");
      TIFFClose(tif);
      return false;
    }
  }
  iHeaderSkip = 0;
  /// @todo FIXME read the component size && n_components from the tag.
  // For now, assuming monochrome, 8-bit data.
  iComponentSize = 8;
  iComponentCount = 1;
  bConvertEndianess = false;
  bSigned = false;
  bIsFloat = false;
  vVolumeAspect[0] = 1;
  vVolumeAspect[1] = 1;
  vVolumeAspect[2] = 1;
  strTitle = "TIFF Volume";
  strSource = SysTools::GetFilename(strSourceFilename); // fixme, move up.
  eType = UVFTables::ES_UNDEFINED;

  strIntermediateFile = strTempDir +
                        SysTools::GetFilename(strSourceFilename) + ".binary";
  LargeRAWFile binary(strIntermediateFile);
  binary.Create(iComponentSize/8 * iComponentCount * vVolumeSize.volume());
  if(!binary.IsOpen()) {
    dbg.Error(_func_, "Could not create binary file %s",
              strIntermediateFile.c_str());
    TIFFClose(tif);
    return false;
  }
  bDeleteIntermediateFile = true;
  do {
    BYTE* slice = tv_read_slice(tif, dbg);
    if(slice) {
      binary.WriteRAW(static_cast<unsigned char*>(slice),
                      vVolumeSize[0]*vVolumeSize[1]*sizeof(BYTE));
      _TIFFfree(slice);
    } else {
      binary.Close();
      binary.Delete();
      TIFFClose(tif);
      return false;
    }
  } while(TIFFReadDirectory(tif));
  binary.Close();

  TIFFClose(tif);
  return true;
}

// unimplemented!
bool
TiffVolumeConverter::ConvertToNative(const std::string&,
                                     const std::string&,
                                     UINT64, UINT64, 
                                     UINT64, bool,
                                     bool,
                                     UINTVECTOR3,
                                     FLOATVECTOR3,
                                     MasterController*,
                                     bool)
{
  return false;
}

// Reads the dimensions of the TIFF volume.  X and Y come from the dimensions
// of the first image in the stack: we assume that this stays constant
// throughout the volume.  Z comes from the number of images in the stack.
static void
tv_dimensions(TIFF *tif, size_t dims[3])
{
  UINT32 x,y;
  size_t z=0;

  TIFFSetDirectory(tif, 0);
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &x);
  // tiff calls the height "length" for some reason.
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &y);
  do {
    ++z;
  } while(TIFFReadDirectory(tif));
  TIFFSetDirectory(tif, 0);

  dims[0] = x;
  dims[1] = y;
  dims[2] = z;
}

_malloc static BYTE*
tv_read_slice(TIFF *tif, AbstrDebugOut& dbg)
{
  BYTE *slice;
  UINT32 width;
  UINT32 height;
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

  dbg.Message(_func_, "Reading %ux%u TIFF slice.", width, height);
  slice = static_cast<BYTE*>(_TIFFmalloc((width*height) * sizeof(BYTE)));
  if(slice == NULL) {
    dbg.Error(_func_, "TIFFmalloc failed.");
    return NULL;
  }
  const tstrip_t n_strips = TIFFNumberOfStrips(tif);
  {
    BYTE *data = slice;
    tdata_t buf = static_cast<tdata_t>(_TIFFmalloc(TIFFStripSize(tif)));
    for(tstrip_t s=0; s < n_strips; ++s) {
      /// @todo FIXME: don't assume the strip is raw; could be encoded.
      /// There's a `compression scheme' tag which probably details this.
      tsize_t n_bytes = TIFFReadRawStrip(tif, s, buf,
                                         static_cast<tsize_t>(-1));
      memcpy(data, buf, n_bytes);
      data += TIFFStripSize(tif);
    }
    _TIFFfree(buf);
  }

  return slice;
}

#if 0
// Converts an RGBA vector `field' to a scalar field of the "vector"'s
// magnitude.  Ignores the alpha component.
_malloc static UINT32*
tv_vector_to_scalar_magnitude(UINT32 *field, size_t w, size_t h)
{
  UINT32* ret = new UINT32[w*h];
  for(size_t i=0; i < w*h; ++i) {
    unsigned char r = field[i] & 0xFF000000;
    unsigned char g = field[i] & 0x00FF0000;
    unsigned char b = field[i] & 0x0000FF00;
    ret[i] = (static_cast<UINT32>(r) +
              static_cast<UINT32>(g) +
              static_cast<UINT32>(b)) / 3;
  }
  return ret;
}
#endif