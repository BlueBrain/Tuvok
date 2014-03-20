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
#pragma once

#ifndef TUVOK_MASTERCONTROLLER_H
#define TUVOK_MASTERCONTROLLER_H

#include "../StdTuvokDefines.h"
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "Basics/PerfCounter.h"
#include "Basics/Vectors.h"
#include "../DebugOut/MultiplexOut.h"
#include "../DebugOut/ConsoleOut.h"

#include "LuaScripting/LuaClassInstance.h"

class SystemInfo;
class IOManager;

namespace tuvok {

class AbstrRenderer;
class GPUMemMan;
class LuaScripting;
class LuaMemberReg;
class LuaIOManagerProxy;
class RenderRegion;

typedef std::deque<AbstrRenderer*> AbstrRendererList;

struct RendererState {
  enum BrickStrategy {
    BS_OnlyNeeded=0, BS_RequestAll, BS_SkipOneLevel, BS_SkipTwoLevels
  };
  BrickStrategy BStrategy;
  uint32_t RehashCount;
  unsigned MDUpdateBehavior;
  unsigned HashTableSize;
  RendererState() :
    BStrategy(BS_RequestAll),
    RehashCount(10),
    MDUpdateBehavior(0),
    HashTableSize(509) {}
};

/** \class MasterController
 * Centralized controller for Tuvok.
 *
 * MasterController is a router for all of the components of
 * Tuvok.  We try to keep modules communicating through this controller,
 * as opposed to directly with other modules.
 * You probably don't want to create an instance directly.  Use the singleton
 * provided by Controller::Instance(). */
class MasterController : public PerfQueryable {
public:

  /// Defaults to using a Console-based debug output stream.
  MasterController();
  virtual ~MasterController();

  /// Deallocate any memory we're holding; rendering or doing any real work is
  /// undefined after this operation.
  void Cleanup();

  /// Lua scripting engine.
  ///@{
  std::shared_ptr<LuaScripting>  LuaScript()       { return m_pLuaScript; }
  ///@}

  /// Add another debug output
  /// \param debugOut      the new stream
  void AddDebugOut(AbstrDebugOut* debugOut);

  /// Removes the given debug output stream.
  /// The stream must be the currently connected/used one.
  void RemoveDebugOut(AbstrDebugOut* debugOut);

  /// Access the currently-active debug stream.
  ///@{
  AbstrDebugOut* DebugOut();
  const AbstrDebugOut *DebugOut() const;
  ///@}

  /// The IO manager is responsible for loading data into host memory.
  ///@{
  IOManager*       IOMan()       { return m_pIOManager;}
  const IOManager& IOMan() const { return *m_pIOManager;}
  ///@}

  /// System information is for looking up host parameters, such as the
  /// amount of memory available.
  ///@{
  SystemInfo*       SysInfo()       { return m_pSystemInfo; }
  const SystemInfo& SysInfo() const { return *m_pSystemInfo; }
  ///@}

  /// Whether or not to expose certain features which aren't actually ready for
  /// users.
  bool ExperimentalFeatures() const;
  void ExperimentalFeatures(bool b);

  /// centralized storage for renderer parameters
  ///@{
  void SetBrickStrategy(size_t strat);
  void SetRehashCount(uint32_t count);
  void SetMDUpdateStrategy(unsigned); ///< takes DM_* enum
  void SetHTSize(unsigned); ///< hash table size

  size_t GetBrickStrategy() const;
  uint32_t GetRehashCount() const;
  unsigned GetMDUpdateStrategy() const;
  unsigned GetHTSize() const;

  void SetMaxCPUMem(uint64_t megs);

  /// return max mem in megabyte
  uint64_t GetMaxCPUMem() const;

  RendererState RState;
  ///@}

  /// Performance query interface.  Each id is a separate performance metric.
  /// @warning Querying a metric resets it!
  double PerfQuery(enum PerfCounter);
  void IncrementPerfCounter(enum PerfCounter, double amount);

private:
  /// Initializer; add all our builtin commands.
  void RegisterLuaCommands();


private:
  SystemInfo*      m_pSystemInfo;
  IOManager*       m_pIOManager;
  MultiplexOut     m_DebugOut;
  ConsoleOut       m_DefaultOut;
  bool             m_bDeleteDebugOutOnExit;
  bool             m_bExperimentalFeatures;

  std::shared_ptr<LuaScripting>       m_pLuaScript;
  std::unique_ptr<LuaMemberReg>       m_pMemReg;
  std::unique_ptr<LuaIOManagerProxy>  m_pIOProxy; 

  /// for PerfCounter tracking.
  double m_Perf[PERF_END];
};

}
#endif // TUVOK_MASTERCONTROLLER_H
