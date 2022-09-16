/** @file
  PEI Services Table Pointer Library for x64.

  The peiservice pointer is stored at first 8-byte of a 4K memory pointed
  by FS BASE.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Register/Intel/ArchitecturalMsr.h>
#include <Library/BaseLib.h>
#include <Library/PeiServicesTablePointerLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PeiServicesLib.h>

/**
  Retrieves the cached value of the PEI Services Table pointer.

  Returns the cached value of the PEI Services Table pointer in a CPU specific manner
  as specified in the CPU binding section of the Platform Initialization Pre-EFI
  Initialization Core Interface Specification.

  If the cached PEI Services Table pointer is NULL, then ASSERT().

  @return  The pointer to PeiServices.

**/
CONST EFI_PEI_SERVICES **
EFIAPI
GetPeiServicesTablePointer (
  VOID
  )
{
  EFI_PEI_SERVICES  ***PeiServices;

  //
  // Note: MSR_IA32_FS_BASE is available in CPU that supports Intel(R) 64
  //       Architecture even CPU runs in 32bit mode.
  //
  PeiServices = (EFI_PEI_SERVICES ***)(UINTN)AsmReadMsr64 (MSR_IA32_FS_BASE);
  ASSERT (PeiServices != NULL);
  return *PeiServices;
}

/**
  Caches a pointer PEI Services Table.

  Caches the pointer to the PEI Services Table specified by PeiServicesTablePointer
  in a CPU specific manner as specified in the CPU binding section of the Platform Initialization
  Pre-EFI Initialization Core Interface Specification.
  The function set the pointer of PEI services immediately preceding the IDT table
  according to PI specification.

  If PeiServicesTablePointer is NULL, then ASSERT().

  @param    PeiServicesTablePointer   The address of PeiServices pointer.
**/
VOID
EFIAPI
SetPeiServicesTablePointer (
  IN CONST EFI_PEI_SERVICES  **PeiServicesTablePointer
  )
{
  EFI_PEI_SERVICES  ***PeiServices;

  ASSERT (PeiServicesTablePointer != NULL);

  //
  // Note: MSR_IA32_FS_BASE is available in CPU that supports Intel(R) 64
  //       Architecture even CPU runs in 32bit mode.
  //
  PeiServices = (EFI_PEI_SERVICES ***)(UINTN)AsmReadMsr64 (MSR_IA32_FS_BASE);
  ASSERT (PeiServices != NULL);

  *PeiServices = (EFI_PEI_SERVICES **)PeiServicesTablePointer;
}

/**
  Perform CPU specific actions required to migrate the PEI Services Table
  pointer from temporary RAM to permanent RAM.

  If The cached PEI Services Table pointer is NULL, then ASSERT().
  If the permanent memory is allocated failed, then ASSERT().
**/
VOID
EFIAPI
MigratePeiServicesTablePointer (
  VOID
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  FsBase;
  EFI_PHYSICAL_ADDRESS  NewFsBase;

  //
  // Allocate the permanent memory.
  //
  Status = PeiServicesAllocatePages (
             EfiBootServicesData,
             1,
             &NewFsBase
             );
  ASSERT_EFI_ERROR (Status);
  //
  // Copy the 1 page from location (stack) allocated in pre-mem to heap in post-mem.
  //
  FsBase = AsmReadMsr64 (MSR_IA32_FS_BASE);
  CopyMem ((VOID *)(UINTN)NewFsBase, (VOID *)(UINTN)FsBase, EFI_PAGES_TO_SIZE (1));
  AsmWriteMsr64 (MSR_IA32_FS_BASE, NewFsBase);
}
