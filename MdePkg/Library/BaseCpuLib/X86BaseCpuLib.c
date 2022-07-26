/** @file
  IsFredEnabled function.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Register/Intel/ArchitecturalMsr.h>

/**
  return if the FRED is enabled

  @retval TRUE    FRED is enabled.
  @retval FALSE   FRED is not enabled.
**/
BOOLEAN
EFIAPI
IsFredEnabled (
  VOID
  )
{
  MSR_IA32_EFER_REGISTER  Efer;
  IA32_CR4                Cr4;

  //
  // Check if in 64bit
  //
  if (sizeof (UINTN) != sizeof (UINT64)) {
    return FALSE;
  }

  //
  // FRED can only be enabled in long mode.
  //
  Efer.Uint64 = AsmReadMsr64 (MSR_IA32_EFER);
  if (Efer.Bits.LMA == 0) {
    return FALSE;
  }

  Cr4.UintN = AsmReadCr4 ();
  return (BOOLEAN)(Cr4.Bits.FRED == 1);
}
