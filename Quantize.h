/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2009 Scientific Computing and Imaging Institute,
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
  \file    Quantize.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Quantization routines.
*/
#pragma once

#ifndef SCIO_QUANTIZE_H
#define SCIO_QUANTIZE_H

#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <functional>
#include <limits>
#ifdef DETECTED_OS_WINDOWS
# include <type_traits>
#else
# include <tr1/type_traits>
#endif
#include <boost/algorithm/minmax_element.hpp>
#include <boost/cstdint.hpp>
#include "IOManager.h"  // for the size defines
#include "Basics/LargeRAWFile.h"

namespace {
/// "Compile time type info"
/// Metaprogramming type traits that we need but aren't in tr1.
/// is_signed: tr1's is_signed only returns true for *integral* signed types..
///            ridiculous.  so we use this as a replacement.
/// size_type: unsigned variant of 'T'.
///@{
template <typename T> struct ctti_base {
  enum { is_signed = std::tr1::is_signed<T>::value ||
                     std::tr1::is_floating_point<T>::value };
};
template <typename T> struct ctti : ctti_base<T> { };

template<> struct ctti<bool> : ctti_base<bool> {
  typedef bool size_type;
};
template<> struct ctti<char> : ctti_base<char> {
  typedef unsigned char size_type;
  typedef signed char signed_type;
};
template<> struct ctti<signed char> : ctti_base<signed char> {
  typedef unsigned char size_type;
  typedef signed char signed_type;
  static const bool is_signed = true;
};
template<> struct ctti<short> : ctti_base<short> {
  typedef unsigned short size_type;
  typedef short signed_type;
};
template<> struct ctti<int> : ctti_base<int> {
  typedef unsigned int size_type;
  typedef int signed_type;
};
template<> struct ctti<boost::int64_t> : ctti_base<boost::int64_t> {
  typedef boost::uint64_t size_type;
  typedef boost::int64_t signed_type;
};
template<> struct ctti<unsigned char> : ctti_base<unsigned char> {
  typedef unsigned char size_type;
  typedef signed char signed_type;
};
template<> struct ctti<unsigned short> : ctti_base<unsigned short> {
  typedef unsigned short size_type;
  typedef short signed_type;
};
template<> struct ctti<unsigned int> : ctti_base<unsigned int> {
  typedef unsigned int size_type;
  typedef int signed_type;
};
#if 1
template<> struct ctti<boost::uint64_t> : ctti_base<boost::uint64_t> {
  typedef boost::uint64_t size_type;
  typedef boost::int64_t signed_type;
};
#endif
template<> struct ctti<UINT64> : ctti_base<UINT64> {
  typedef UINT64 size_type;
  typedef INT64 signed_type;
};
template<> struct ctti<float> : ctti_base<float> {
  typedef float size_type;
  typedef float signed_type;
};
template<> struct ctti<double> : ctti_base<double> {
  typedef double size_type;
  typedef double signed_type;
};
///@}
};

/// Progress policies.  Must implement a constructor and `notify' method which
/// both take a `T'.  The constructor is given the max value; the notify method
/// is given the current value.
///    NullProgress -- nothing, use when you don't care.
///    TuvokProgress -- forward progress info to Tuvok debug logs.
///@{
template <typename T>
struct NullProgress {
  NullProgress() {};
  NullProgress(T) {};
  static void notify(T) {}
};
template <typename T>
struct TuvokProgress {
  TuvokProgress(T total) : tMax(total) {}
  void notify(T current) const {
    MESSAGE("Computing value range (%5.3f%% complete).",
            static_cast<double>(current) / static_cast<double>(tMax)*100.0);
  }
  private: T tMax;
};
///@}

/// Data source policies.  Must implement:
///   constructor: takes an opened file.
///   size(): returns the number of elements in the file.
///   read(data, b): reads `b' bytes in to `data'.  Returns number of elems
///                  actually read.
/// ios_data_src -- data source for C++ iostreams.
/// raw_data_src -- data source for Basics' LargeRAWFile.
///@{
template <typename T>
struct ios_data_src {
  ios_data_src(std::ifstream& fs) : ifs(fs) {
    if(!ifs.is_open()) {
      throw std::runtime_error(__FILE__);
    }
  }

  boost::uint64_t size() {
    std::streampos cur = ifs.tellg();
    ifs.seekg(0, std::ios::end);
    boost::uint64_t retval = ifs.tellg();
    ifs.seekg(cur, std::ios::beg);
    return retval/sizeof(T);
  }
  size_t read(unsigned char *data, size_t max_bytes) {
    ifs.read(reinterpret_cast<char*>(data), std::streamsize(max_bytes));
    return ifs.gcount()/sizeof(T);
  }
  private:
    std::ifstream& ifs;
    const char *filename;
};

template <typename T>
struct raw_data_src {
  raw_data_src(LargeRAWFile& r) : raw(r) {
    if(!raw.IsOpen()) {
      throw std::runtime_error(__FILE__);
    }
  }

  boost::uint64_t size() { return raw.GetCurrentSize() / sizeof(T); }
  size_t read(unsigned char *data, size_t max_bytes) {
    return raw.ReadRAW(data, max_bytes)/sizeof(T);
  }
  private: LargeRAWFile& raw;
};

template <typename T>
struct multi_raw_data_src {
  multi_raw_data_src(std::vector<LargeRAWFile> f) : files(f), cur_file(0),
                                                    total_size(0) {
    for(std::vector<LargeRAWFile>::const_iterator rf = files.begin();
        rf != files.end(); ++rf) {
      if(!rf->IsOpen()) {
        throw std::runtime_error(__FILE__ ": file not open");
      }
    }
  }

  boost::uint64_t size() {
    if(total_size == 0) {
      for(std::vector<LargeRAWFile>::iterator rf = files.begin();
          rf != files.end(); ++rf) {
        total_size += rf->GetCurrentSize() / sizeof(T);
      }
    }
    return total_size;
  }

  size_t read(unsigned char *data, size_t max_bytes) {
    size_t bytes = files[cur_file].ReadRAW(data, max_bytes);
    while(bytes == 0) {
      cur_file++;
      if(cur_file >= files.size()) { return 0; }
      bytes = files[cur_file].ReadRAW(data, max_bytes);
    }
    return bytes / sizeof(T);
  }

  private:
    std::vector<LargeRAWFile> files;
    size_t cur_file;
    boost::uint64_t total_size;
};
///@}

/// If we just do a standard "is the value of type T <= 4096?" then compilers
/// complain with 8bit types because the comparison is always true.  Great,
/// thanks, I didn't know that.  So, this basically gets around a warning.
/// I love C++ sometimes.
namespace { namespace Fits {
  template<typename T, size_t sz> bool inXBits(T v) {
    return v <= static_cast<T>(sz);
  }
  template<> bool inXBits<signed char, 256>(signed char) { return true; }
  template<> bool inXBits<signed char, 4096>(signed char) { return true; }
}; };

/// Histogram policies.  minmax can sometimes compute a 1D histogram as it
/// marches over the data.  It may happen that the data must be quantized
/// though, forcing the histogram to be recalculated.
/// Must implement:
///    bin(T): bin the given value.  return false if we shouldn't bother
///            computing the histogram anymore.
template<typename T> struct NullHistogram {
  static bool bin(T) { return false; }
};
// Calculate a 12Bit histogram, but when we encounter a value which does not
// fit (i.e., we know we'll need to quantize), don't bother anymore.
template<typename T, size_t sz>
struct UnsignedHistogram {
  UnsignedHistogram(std::vector<UINT64>& h)
    : histo(h), calculate(true) {}

  bool bin(T value) {
    if(!calculate || !Fits::inXBits<T, sz>(value)) {
      calculate = false;
    } else {
      update(value);
    }
    return calculate;
  }

  void update(T value) {
    // Calculate our bias factor up front.
    typename ctti<T>::size_type bias;
    bias = static_cast<typename ctti<T>::size_type>
                      (std::fabs(static_cast<double>
                                 (-(std::numeric_limits<T>::max()))));

    typename ctti<T>::size_type u_value;
    u_value = ctti<T>::is_signed ? value + bias : value;
    // Either the data are unsigned, or there exist no values s.t. the value
    // plus the bias is negative (and therefore *this* value + the bias is
    // nonnegative).
    // Unfortunately we can't assert this since compilers are too dumb: with
    // an unsigned T, they complain that "value+bias >= 0 is always true",
    // despite the fact that the comparison would never happen for unsigned
    // values.
//  assert(!ctti<T>::is_signed || (ctti<T>::is_signed && ((value+bias) >= 0)));

    if(u_value < histo.size()) {
      ++histo[static_cast<size_t>(u_value)];
    }
  }
  private:
    std::vector<UINT64>& histo;
    bool calculate;
};

/// Computes the minimum and maximum of a conceptually one dimensional dataset.
/// Takes policies tell it how to access data && notify external entities of
/// progress.
template <typename T, size_t sz,
          template <typename T> class DataSrc,
          template <typename T, size_t> class Histogram,
          class Progress>
std::pair<T,T> io_minmax(DataSrc<T> ds, Histogram<T, sz> histogram,
                         const Progress& progress, size_t iCurrentIncoreSize)
{
  std::vector<T> data(iCurrentIncoreSize);
  boost::uint64_t iPos = 0;
  boost::uint64_t iSize = ds.size();

  // Default min is the max value representable by the data type.  Default max
  // is the smallest value representable by the data type.
  std::pair<T,T> t_minmax(std::numeric_limits<T>::max(),
                          -(std::numeric_limits<T>::max()+1));
  // ... but if the data type is unsigned, the correct default 'max' is 0.
  if(!ctti<T>::is_signed) {
    t_minmax.second = std::numeric_limits<T>::min(); // ... == 0.
  }

  while(iPos < iSize) {
    size_t n_records = ds.read((unsigned char*)(&(data.at(0))), iCurrentIncoreSize);
    data.resize(n_records);
    if(n_records == 0) { break; } // bail out if the read gave us nothing.

    iPos += boost::uint64_t(n_records);
    progress.notify(iPos);

    typedef typename std::vector<T>::const_iterator iterator;
    std::pair<iterator,iterator> cur_mm = boost::minmax_element(data.begin(),
                                                                data.end());
    t_minmax.first = std::min(t_minmax.first, *cur_mm.first);
    t_minmax.second = std::max(t_minmax.second, *cur_mm.second);

    // Run over the data again and bin the data for the histogram.
    for(size_t i=0; i < n_records && histogram.bin(data[i]); ++i) { }
  }
  return t_minmax;
}

#endif // SCIO_QUANTIZE_H
