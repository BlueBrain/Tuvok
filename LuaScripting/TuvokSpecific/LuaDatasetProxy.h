/*
 For more information, please see: http://software.sci.utah.edu

 The MIT License

 Copyright (c) 2012 Scientific Computing and Imaging Institute,
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
 \brief A Lua class proxy for IO's dataset class.
 */

#ifndef LUADATASETPROXY_H_
#define LUADATASETPROXY_H_

#include "../LuaScripting.h"
#include "../LuaClassRegistration.h"

namespace tuvok
{

class Dataset;

class LuaDatasetProxy
{
public:
  LuaDatasetProxy();
  virtual ~LuaDatasetProxy();

  void bindDataset(Dataset* ds);

  LuaDatasetProxy* luaDatasetCons() {return new LuaDatasetProxy;}
  void defineLuaInterface(LuaClassRegistration<LuaDatasetProxy>& reg,
                          LuaDatasetProxy* me,
                          LuaScripting* ss);

private:

  /// Class registration we received from defineLuaInterface.
  LuaClassRegistration<LuaDatasetProxy>* mReg;
};

} /* namespace tuvok */
#endif /* LUADATASETPROXY_H_ */