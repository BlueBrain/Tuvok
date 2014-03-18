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

//!    File   : MasterController.cpp
//!    Author : Jens Krueger
//!             SCI Institute
//!             University of Utah
//!    Date   : September 2008
//
//!    Copyright (C) 2008 SCI Institute

#include <algorithm>
#include <sstream>
#include <functional>
#include "MasterController.h"
#include "../Basics/SystemInfo.h"
#include "../Basics/SysTools.h"
#include "../IO/IOManager.h"
#include "../IO/TransferFunction1D.h"
#include "../IO/Dataset.h"
#include "../IO/AbstrConverter.h"

#include "../LuaScripting/LuaScripting.h"
#include "../LuaScripting/LuaMemberReg.h"
#include "../LuaScripting/TuvokSpecific/LuaTuvokTypes.h"
#include "../LuaScripting/TuvokSpecific/LuaDatasetProxy.h"
#include "../LuaScripting/TuvokSpecific/LuaTransferFun1DProxy.h"
#include "../LuaScripting/TuvokSpecific/LuaTransferFun2DProxy.h"
#include "../LuaScripting/TuvokSpecific/LuaIOManagerProxy.h"
#include "../LuaScripting/TuvokSpecific/MatrixMath.h"

using namespace tuvok;

MasterController::MasterController() :
  m_bDeleteDebugOutOnExit(false),
  m_bExperimentalFeatures(false),
  m_pLuaScript(new LuaScripting()),
  m_pMemReg(new LuaMemberReg(m_pLuaScript))
{
  m_pSystemInfo   = new SystemInfo();
  m_pIOManager    = new IOManager();

  using namespace std::placeholders;

  RegisterLuaCommands();

  // Register IO commands.
  m_pIOProxy = std::unique_ptr<LuaIOManagerProxy>(
      new LuaIOManagerProxy(m_pIOManager, m_pLuaScript));

  // TEMPORARY -- Disable the provenance system.
  LuaScript()->cexec("provenance.enable", false);

  RState.BStrategy = RendererState::BS_SkipTwoLevels;
  std::fill(m_Perf, m_Perf+PERF_END, 0.0);
}


MasterController::~MasterController() {
  Cleanup();
  m_DebugOut.clear();
}

void MasterController::Cleanup() {

  delete m_pSystemInfo;
  m_pSystemInfo = NULL;
  delete m_pIOManager;
  m_pIOManager = NULL;
}


void MasterController::AddDebugOut(AbstrDebugOut* debugOut) {
  if (debugOut != NULL) {
    m_DebugOut.Other(_func_, "Disconnecting from this debug out");

    m_DebugOut.AddDebugOut(debugOut);

    debugOut->Other(_func_, "Connected to this debug out");
  } else {
    m_DebugOut.Warning(_func_,
                       "New debug is a NULL pointer, ignoring it.");
  }
}


void MasterController::RemoveDebugOut(AbstrDebugOut* debugOut) {
  m_DebugOut.RemoveDebugOut(debugOut);
}

/// Access the currently-active debug stream.
AbstrDebugOut* MasterController::DebugOut()
{
  return (m_DebugOut.empty()) ? static_cast<AbstrDebugOut*>(&m_DefaultOut)
                                  : static_cast<AbstrDebugOut*>(&m_DebugOut);
}
const AbstrDebugOut *MasterController::DebugOut() const {
  return (m_DebugOut.empty())
           ? static_cast<const AbstrDebugOut*>(&m_DefaultOut)
           : static_cast<const AbstrDebugOut*>(&m_DebugOut);
}

void MasterController::SetBrickStrategy(size_t strat) {
  assert(strat <= RendererState::BS_SkipTwoLevels); // hack
  this->RState.BStrategy = static_cast<RendererState::BrickStrategy>(strat);
}
void MasterController::SetRehashCount(uint32_t n) {
  this->RState.RehashCount = n;
}
void MasterController::SetMDUpdateStrategy(unsigned strat) {
  this->RState.MDUpdateBehavior = strat;
}
void MasterController::SetHTSize(unsigned size) {
  this->RState.HashTableSize = size;
}

size_t MasterController::GetBrickStrategy() const {
  return static_cast<size_t>(this->RState.BStrategy);
}
uint32_t MasterController::GetRehashCount() const {
  return this->RState.RehashCount;
}
unsigned MasterController::GetMDUpdateStrategy() const {
  return static_cast<unsigned>(this->RState.MDUpdateBehavior);
}
unsigned MasterController::GetHTSize() const {
  return this->RState.HashTableSize;
}

uint64_t MasterController::GetMaxCPUMem() const {
  const uint64_t megabyte = 1024 * 1024;
  return m_pSystemInfo->GetMaxUsableCPUMem() / megabyte;
}

void MasterController::SetMaxCPUMem(uint64_t megs) {
  const uint64_t megabyte = 1024 * 1024;
  m_pSystemInfo->SetMaxUsableCPUMem(megabyte * megs);
}

double MasterController::PerfQuery(enum PerfCounter pc) {
  assert(pc < PERF_END);
  double tmp = m_Perf[pc];
  m_Perf[pc] = 0.0;
  return tmp;
}
void MasterController::IncrementPerfCounter(enum PerfCounter pc,
                                            double amount) {
  assert(pc < PERF_END);
  m_Perf[pc] += amount;
}

void register_unsigned(lua_State* lua, const char* name, unsigned value) {
  lua_pushinteger(lua, value);
  lua_setglobal(lua, name);
}

void register_perf_enum(std::shared_ptr<LuaScripting>& ss) {
  lua_State* lua = ss->getLuaState();
  register_unsigned(lua, "PERF_SUBFRAMES", PERF_SUBFRAMES);
  register_unsigned(lua, "PERF_RENDER", PERF_RENDER);
  register_unsigned(lua, "PERF_RAYCAST", PERF_RAYCAST);
  register_unsigned(lua, "PERF_READ_HTABLE", PERF_READ_HTABLE);
  register_unsigned(lua, "PERF_CONDENSE_HTABLE", PERF_CONDENSE_HTABLE);
  register_unsigned(lua, "PERF_SORT_HTABLE", PERF_SORT_HTABLE);
  register_unsigned(lua, "PERF_UPLOAD_BRICKS", PERF_UPLOAD_BRICKS);
  register_unsigned(lua, "PERF_POOL_SORT", PERF_POOL_SORT);
  register_unsigned(lua, "PERF_POOL_UPLOADED_MEM", PERF_POOL_UPLOADED_MEM);
  register_unsigned(lua, "PERF_POOL_GET_BRICK", PERF_POOL_GET_BRICK);
  register_unsigned(lua, "PERF_DY_GET_BRICK", PERF_DY_GET_BRICK);
  register_unsigned(lua, "PERF_DY_CACHE_LOOKUPS", PERF_DY_CACHE_LOOKUPS);
  register_unsigned(lua, "PERF_DY_CACHE_LOOKUP", PERF_DY_CACHE_LOOKUP);
  register_unsigned(lua, "PERF_DY_RESERVE_BRICK", PERF_DY_RESERVE_BRICK);
  register_unsigned(lua, "PERF_DY_LOAD_BRICK", PERF_DY_LOAD_BRICK);
  register_unsigned(lua, "PERF_DY_CACHE_ADDS", PERF_DY_CACHE_ADDS);
  register_unsigned(lua, "PERF_DY_CACHE_ADD", PERF_DY_CACHE_ADD);
  register_unsigned(lua, "PERF_DY_BRICK_COPIED", PERF_DY_BRICK_COPIED);
  register_unsigned(lua, "PERF_DY_BRICK_COPY", PERF_DY_BRICK_COPY);
  register_unsigned(lua, "PERF_POOL_UPLOAD_BRICK", PERF_POOL_UPLOAD_BRICK);
  register_unsigned(lua, "PERF_POOL_UPLOAD_TEXEL", PERF_POOL_UPLOAD_TEXEL);
  register_unsigned(lua, "PERF_POOL_UPLOAD_METADATA", PERF_POOL_UPLOAD_METADATA);
  register_unsigned(lua, "PERF_EO_BRICKS", PERF_EO_BRICKS);
  register_unsigned(lua, "PERF_EO_DISK_READ", PERF_EO_DISK_READ);
  register_unsigned(lua, "PERF_EO_DECOMPRESSION", PERF_EO_DECOMPRESSION);

  register_unsigned(lua, "PERF_MM_PRECOMPUTE", PERF_MM_PRECOMPUTE);
  register_unsigned(lua, "PERF_SOMETHING", PERF_SOMETHING);
}

void MasterController::RegisterLuaCommands() {
  std::shared_ptr<LuaScripting> ss = LuaScript();


  // Register dataset proxy
  ss->registerClassStatic<LuaDatasetProxy>(
      &LuaDatasetProxy::luaConstruct,
      "tuvok.datasetProxy",
      "Constructs a dataset proxy.",
      LuaClassRegCallback<LuaDatasetProxy>::Type(
          LuaDatasetProxy::defineLuaInterface));

  // Register 1D transfer function proxy
  ss->registerClassStatic<LuaTransferFun1DProxy>(
      &LuaTransferFun1DProxy::luaConstruct,
      "tuvok.transferFun1D",
      "Constructs a 1D transfer function proxy. Construction of these proxies "
      "should be left to the renderer.",
      LuaClassRegCallback<LuaTransferFun1DProxy>::Type(
          LuaTransferFun1DProxy::defineLuaInterface));

  // Register 2D transfer function proxy
  ss->registerClassStatic<LuaTransferFun2DProxy>(
      &LuaTransferFun2DProxy::luaConstruct,
      "tuvok.transferFun2D",
      "Constructs a 2D transfer function proxy. Construction of these proxies "
      "should be left to the renderer.",
      LuaClassRegCallback<LuaTransferFun2DProxy>::Type(
          LuaTransferFun2DProxy::defineLuaInterface));

  // register matrix math functions.
  tuvok::registrar::matrix_math(ss);

  m_pMemReg->registerFunction(this,
    &MasterController::SetBrickStrategy, "tuvok.state.brickStrategy", "",
    false
  );
  m_pMemReg->registerFunction(this,
    &MasterController::SetRehashCount, "tuvok.state.rehashCount", "",
    false
  );
  m_pMemReg->registerFunction(this, &MasterController::SetMDUpdateStrategy,
    "tuvok.state.mdUpdateStrategy", "control the background metadata update "
    "thread.\n  0: enabled (default)\n  1: async thread does nothing\n  2: "
    "async thread is never even created.\n  3: skip all metadata computations;"
    " effectively disables empty space leaping.", false);
  m_pMemReg->registerFunction(this, &MasterController::SetHTSize,
    "tuvok.state.hashTableSize", "sets the size of the hash table.  Larger "
    "values give better raw performance; smaller values make the system more "
    " responsive.  default: 509", false);

  m_pMemReg->registerFunction(this, &MasterController::GetBrickStrategy,
    "tuvok.state.getBrickStrategy", "", false);
  m_pMemReg->registerFunction(this, &MasterController::GetRehashCount,
    "tuvok.state.getRehashCount", "", false);
  m_pMemReg->registerFunction(this, &MasterController::GetMDUpdateStrategy,
    "tuvok.state.getMdUpdateStrategy", "", false);
  m_pMemReg->registerFunction(this, &MasterController::GetHTSize,
    "tuvok.state.getHashTableSize", "", false);

  m_pMemReg->registerFunction(this,
    &MasterController::PerfQuery, "tuvok.perf",
    "queries performance information.  meaning is query-specific.", false
  );
  ss->registerFunction(&SysTools::basename, "basename",
                       "basename for the given filename", false);
  ss->registerFunction(&SysTools::dirname, "dirname",
                       "dirname for the given filename", false);
  register_perf_enum(ss);

  // Register Tuvok specific math functions...
  LuaMathFunctions::registerMathFunctions(ss);
}

bool MasterController::ExperimentalFeatures() const {
  return m_bExperimentalFeatures;
}

void MasterController::ExperimentalFeatures(bool b) {
  m_bExperimentalFeatures = b;
}
