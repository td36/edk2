/** @file
  CPU exception handler library implemenation for DXE modules.

  Copyright (c) 2013 - 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuExceptionCommon.h"
#include <Library/DebugLib.h>

#define FRED_ALIGNMENT_REQUIREMENT  64

/**
  Initializes FRED exceptions entry and provides the default exception handlers.

  @retval EFI_SUCCESS    The FRED exceptions have been successfully initialized .
**/
EFI_STATUS
FredInitialize (
  VOID
  )
{
  UINT64  FredEntry4kAlign;

  FredEntry4kAlign = ALIGN_VALUE ((UINT64)(UINTN)AsmFredEntry, SIZE_4KB);
  DEBUG ((EFI_D_INFO, "FredEntry4kAlign = 0x%lx\n", FredEntry4kAlign));
  AsmWriteMsr64 (IA32_FRED_CONFIG, FredEntry4kAlign);
  AsmWriteMsr64 (IA32_FRED_STKLVLS, 0);
  return EFI_SUCCESS;
}

/**
  Setup separate stacks for certain exception handlers for FRED.

  @param[in]  Buffer        Point to buffer used to separate exception stack.
  @param[in]  BufferSize    On input, it indicates the byte size of Buffer. If the
                            size is not enough, the return status will be
                            EFI_BUFFER_TOO_SMALL, and output BufferSize will be
                            the size it needs.

  @retval EFI_SUCCESS             The stacks are assigned successfully.
  @retval EFI_BUFFER_TOO_SMALL    This BufferSize is too small.
**/
EFI_STATUS
FredInitializeSeparateExceptionStacks (
  IN     VOID   *Buffer,
  IN OUT UINTN  *BufferSize
  )
{
  UINTN   NeedBufferSize;
  UINT64  Stack1;
  UINT64  ExceptionStackLevels;

  ASSERT (BufferSize != NULL);
  //
  // Best known config is to only let double fault have a separate stack
  //
  ExceptionStackLevels = 1 << (EXCEPT_IA32_DOUBLE_FAULT * 2);
  NeedBufferSize       = CPU_KNOWN_GOOD_STACK_SIZE + FRED_ALIGNMENT_REQUIREMENT - 1;

  if (*BufferSize < NeedBufferSize) {
    *BufferSize = NeedBufferSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Stack1 = (UINT64)(UINTN)Buffer + *BufferSize;
  //
  // Best known config is to only set IA32_FRED_RSP1 for double fault
  // Make sure stack top is 64 bytes align
  //
  AsmWriteMsr64 (IA32_FRED_RSP1, ALIGN_VALUE (Stack1 - FRED_ALIGNMENT_REQUIREMENT + 1, FRED_ALIGNMENT_REQUIREMENT));
  AsmWriteMsr64 (IA32_FRED_STKLVLS, ExceptionStackLevels);
  return EFI_SUCCESS;
}
