/** @file
  This file defines a VARIABLE_RUNTIME_CACHE_CONTEXT_HOB.
  It provides the variable runtime cache context.

  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef VARIABLE_RUNTIME_CACHE_CONTEXT_H_
#define VARIABLE_RUNTIME_CACHE_CONTEXT_H_

#include <PiPei.h>

#define VARIABLE_RUNTIME_CACHE_CONTEXT_HOB_REVISION  1

#define VARIABLE_RUNTIME_CACHE_CONTEXT_GUID \
  { \
    0x0f472f7d, 0x6713, 0x4915, {0x96, 0x14, 0x5d, 0xda, 0x28, 0x40, 0x10, 0x56}  \
  }

typedef struct {
  ///
  /// TRUE indicates GetVariable () or GetNextVariable () is being called.
  /// When the value is FALSE, the given update (and any other pending updates)
  /// can be flushed to the runtime cache.
  ///
  BOOLEAN    ReadLock;
  ///
  /// TRUE indicates there is pending update for the given variable store needed
  /// to be flushed to the runtime cache.
  ///
  BOOLEAN    PendingUpdate;
  ///
  /// When the BOOLEAN variable value is ture, it indicates all HOB variables
  /// have been flushed in flash.
  ///
  BOOLEAN    HobFlushComplete;
} CACHE_INFO_FLAG;

typedef struct {
  CACHE_INFO_FLAG    *CacheInfoFlag;
  ///
  /// Buffer reserved for runtime Hob cache
  ///
  UINT64             RuntimeHobCacheBuffer;
  UINT64             RuntimeHobCachePages;
  ///
  /// Buffer reserved for Non-Volatile runtime cache
  ///
  UINT64             RuntimeNvCacheBuffer;
  UINT64             RuntimeNvCachePages;
  ///
  /// Buffer reserved for Volatile runtime cache
  ///
  UINT64             RuntimeVolatileCacheBuffer;
  UINT64             RuntimeVolatileCachePages;
} VARIABLE_RUNTIME_CACHE_CONTEXT_HOB;

extern EFI_GUID  gEdkiiVariableRuntimeCacheContextHobGuid;

#endif
