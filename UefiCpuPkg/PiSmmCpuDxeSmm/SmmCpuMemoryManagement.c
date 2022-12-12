/** @file

Copyright (c) 2016 - 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PiSmmCpuDxeSmm.h"

//
// attributes for reserved memory before it is promoted to system memory
//
#define EFI_MEMORY_PRESENT      0x0100000000000000ULL
#define EFI_MEMORY_INITIALIZED  0x0200000000000000ULL
#define EFI_MEMORY_TESTED       0x0400000000000000ULL

#define PREVIOUS_MEMORY_DESCRIPTOR(MemoryDescriptor, Size) \
  ((EFI_MEMORY_DESCRIPTOR *)((UINT8 *)(MemoryDescriptor) - (Size)))

EFI_MEMORY_DESCRIPTOR  *mUefiMemoryMap;
UINTN                  mUefiMemoryMapSize;
UINTN                  mUefiDescriptorSize;

EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *mGcdMemSpace       = NULL;
UINTN                            mGcdMemNumberOfDesc = 0;

EFI_MEMORY_ATTRIBUTES_TABLE  *mUefiMemoryAttributesTable = NULL;

BOOLEAN      mIsShadowStack      = FALSE;
BOOLEAN      m5LevelPagingNeeded = FALSE;
PAGING_MODE  mPagingMode         = PagingModeMax;

//
// Global variable to keep track current available memory used as page table.
//
PAGE_TABLE_POOL  *mPageTablePool = NULL;

/**
  Initialize a buffer pool for page table use only.

  To reduce the potential split operation on page table, the pages reserved for
  page table should be allocated in the times of PAGE_TABLE_POOL_UNIT_PAGES and
  at the boundary of PAGE_TABLE_POOL_ALIGNMENT. So the page pool is always
  initialized with number of pages greater than or equal to the given PoolPages.

  Once the pages in the pool are used up, this method should be called again to
  reserve at least another PAGE_TABLE_POOL_UNIT_PAGES. But usually this won't
  happen in practice.

  @param PoolPages  The least page number of the pool to be created.

  @retval TRUE    The pool is initialized successfully.
  @retval FALSE   The memory is out of resource.
**/
BOOLEAN
InitializePageTablePool (
  IN UINTN  PoolPages
  )
{
  VOID                *Buffer;
  PAGING_MODE         PagingMode;
  UINTN               PageTableBufferSize;
  UINTN               PageTable;
  VOID                *PageTableBuffer;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;

  //
  // Always reserve at least PAGE_TABLE_POOL_UNIT_PAGES, including one page for
  // header.
  //
  PoolPages += 1;   // Add one page for header.
  PoolPages  = ((PoolPages - 1) / PAGE_TABLE_POOL_UNIT_PAGES + 1) *
               PAGE_TABLE_POOL_UNIT_PAGES;
  Buffer = AllocateAlignedPages (PoolPages, PAGE_TABLE_POOL_ALIGNMENT);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "ERROR: Out of aligned pages\r\n"));
    return FALSE;
  }

  //
  // Link all pools into a list for easier track later.
  //
  if (mPageTablePool == NULL) {
    mPageTablePool           = Buffer;
    mPageTablePool->NextPool = mPageTablePool;
  } else {
    ((PAGE_TABLE_POOL *)Buffer)->NextPool = mPageTablePool->NextPool;
    mPageTablePool->NextPool              = Buffer;
    mPageTablePool                        = Buffer;
  }

  //
  // Reserve one page for pool header.
  //
  mPageTablePool->FreePages = PoolPages - 1;
  mPageTablePool->Offset    = EFI_PAGES_TO_SIZE (1);

  //
  // If access to non-SMRAM is restricted and mSmmReadyToLock is TRUE,
  // Mark the new pool pages as read-only.
  //
  if (mSmmReadyToLock && IsRestrictedMemoryAccess () && IfSetPageTableReadOnly ()) {
    SmmSetMemoryAttributes (Buffer, EFI_PAGES_TO_SIZE (PoolPages), EFI_MEMORY_RO);
  }

  return TRUE;
}

/**
  Return page table entry to match the address.

  @param[in]   PageTableBase      The page table base.
  @param[in]   Enable5LevelPaging If PML5 paging is enabled.
  @param[in]   Address            The address to be checked.
  @param[out]  PageAttributes     The page attribute of the page entry.

  @return The page entry.
**/
VOID *
GetPageTableEntry (
  IN  UINTN             PageTableBase,
  IN  BOOLEAN           Enable5LevelPaging,
  IN  PHYSICAL_ADDRESS  Address,
  OUT PAGE_ATTRIBUTE    *PageAttribute
  )
{
  UINTN   Index1;
  UINTN   Index2;
  UINTN   Index3;
  UINTN   Index4;
  UINTN   Index5;
  UINT64  *L1PageTable;
  UINT64  *L2PageTable;
  UINT64  *L3PageTable;
  UINT64  *L4PageTable;
  UINT64  *L5PageTable;

  Index5 = ((UINTN)RShiftU64 (Address, 48)) & PAGING_PAE_INDEX_MASK;
  Index4 = ((UINTN)RShiftU64 (Address, 39)) & PAGING_PAE_INDEX_MASK;
  Index3 = ((UINTN)Address >> 30) & PAGING_PAE_INDEX_MASK;
  Index2 = ((UINTN)Address >> 21) & PAGING_PAE_INDEX_MASK;
  Index1 = ((UINTN)Address >> 12) & PAGING_PAE_INDEX_MASK;

  if (sizeof (UINTN) == sizeof (UINT64)) {
    if (Enable5LevelPaging) {
      L5PageTable = (UINT64 *)PageTableBase;
      if (L5PageTable[Index5] == 0) {
        *PageAttribute = PageNone;
        return NULL;
      }

      L4PageTable = (UINT64 *)(UINTN)(L5PageTable[Index5] & ~mAddressEncMask & PAGING_4K_ADDRESS_MASK_64);
    } else {
      L4PageTable = (UINT64 *)PageTableBase;
    }

    if (L4PageTable[Index4] == 0) {
      *PageAttribute = PageNone;
      return NULL;
    }

    L3PageTable = (UINT64 *)(UINTN)(L4PageTable[Index4] & ~mAddressEncMask & PAGING_4K_ADDRESS_MASK_64);
  } else {
    L3PageTable = (UINT64 *)PageTableBase;
  }

  if (L3PageTable[Index3] == 0) {
    *PageAttribute = PageNone;
    return NULL;
  }

  if ((L3PageTable[Index3] & IA32_PG_PS) != 0) {
    // 1G
    *PageAttribute = Page1G;
    return &L3PageTable[Index3];
  }

  L2PageTable = (UINT64 *)(UINTN)(L3PageTable[Index3] & ~mAddressEncMask & PAGING_4K_ADDRESS_MASK_64);
  if (L2PageTable[Index2] == 0) {
    *PageAttribute = PageNone;
    return NULL;
  }

  if ((L2PageTable[Index2] & IA32_PG_PS) != 0) {
    // 2M
    *PageAttribute = Page2M;
    return &L2PageTable[Index2];
  }

  // 4k
  L1PageTable = (UINT64 *)(UINTN)(L2PageTable[Index2] & ~mAddressEncMask & PAGING_4K_ADDRESS_MASK_64);
  if ((L1PageTable[Index1] == 0) && (Address != 0)) {
    *PageAttribute = PageNone;
    return NULL;
  }

  *PageAttribute = Page4K;
  return &L1PageTable[Index1];
}

/**
  Return memory attributes of page entry.

  @param[in]  PageEntry        The page entry.

  @return Memory attributes of page entry.
**/
UINT64
GetAttributesFromPageEntry (
  IN  UINT64  *PageEntry
  )
{
  UINT64  Attributes;

  Attributes = 0;
  if ((*PageEntry & IA32_PG_P) == 0) {
    Attributes |= EFI_MEMORY_RP;
  }

  if ((*PageEntry & IA32_PG_RW) == 0) {
    Attributes |= EFI_MEMORY_RO;
  }

  if ((*PageEntry & IA32_PG_NX) != 0) {
    Attributes |= EFI_MEMORY_XP;
  }

  return Attributes;
}

/**
  This function modifies the page attributes for the memory region specified by BaseAddress and
  Length from their current attributes to the attributes specified by Attributes.

  Caller should make sure BaseAddress and Length is at page boundary.

  @param[in]   PageTableBase    The page table base.
  @param[in]   BaseAddress      The physical address that is the start address of a memory region.
  @param[in]   Length           The size in bytes of the memory region.
  @param[in]   Attributes       The bit mask of attributes to modify for the memory region.
  @param[in]   IsSet            TRUE means to set attributes. FALSE means to clear attributes.
  @param[out]  IsModified       TRUE means page table modified. FALSE means page table not modified.

  @retval RETURN_SUCCESS           The attributes were modified for the memory region.
  @retval RETURN_ACCESS_DENIED     The attributes for the memory resource range specified by
                                   BaseAddress and Length cannot be modified.
  @retval RETURN_INVALID_PARAMETER Length is zero.
                                   Attributes specified an illegal combination of attributes that
                                   cannot be set together.
  @retval RETURN_OUT_OF_RESOURCES  There are not enough system resources to modify the attributes of
                                   the memory resource range.
  @retval RETURN_UNSUPPORTED       The processor does not support one or more bytes of the memory
                                   resource range specified by BaseAddress and Length.
                                   The bit mask of attributes is not support for the memory resource
                                   range specified by BaseAddress and Length.
**/
RETURN_STATUS
ConvertMemoryPageAttributes (
  IN  UINTN             PageTableBase,
  IN  PAGING_MODE       PagingMode,
  IN  PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64            Length,
  IN  UINT64            Attributes,
  IN  BOOLEAN           IsSet,
  OUT BOOLEAN           *IsModified   OPTIONAL
  )
{
  RETURN_STATUS         Status;
  IA32_MAP_ATTRIBUTE    PagingAttribute;
  IA32_MAP_ATTRIBUTE    PagingAttrMask;
  UINTN                 PageTableBufferSize;
  VOID                  *PageTableBuffer;
  EFI_PHYSICAL_ADDRESS  MaximumSupportMemAddress;

  ASSERT (Attributes != 0);
  ASSERT ((Attributes & ~EFI_MEMORY_ATTRIBUTE_MASK) == 0);

  ASSERT ((BaseAddress & (SIZE_4KB - 1)) == 0);
  ASSERT ((Length & (SIZE_4KB - 1)) == 0);
  ASSERT (PageTableBase != 0);

  if (Length == 0) {
    return RETURN_INVALID_PARAMETER;
  }

  MaximumSupportMemAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)(LShiftU64 (1, mPhysicalAddressBits) - 1);
  if (BaseAddress > MaximumSupportMemAddress) {
    return RETURN_UNSUPPORTED;
  }

  if (Length > MaximumSupportMemAddress) {
    return RETURN_UNSUPPORTED;
  }

  if ((Length != 0) && (BaseAddress > MaximumSupportMemAddress - (Length - 1))) {
    return RETURN_UNSUPPORTED;
  }

  PagingAttribute.Uint64 = 0;
  PagingAttrMask.Uint64  = 0;

  if ((Attributes & EFI_MEMORY_RP) != 0) {
    PagingAttribute.Bits.Present = IsSet ? 0 : 1;
    PagingAttrMask.Bits.Present  = 1;
  }

  if ((Attributes & EFI_MEMORY_RO) != 0) {
    PagingAttrMask.Bits.ReadWrite = 1;
    if (IsSet) {
      PagingAttribute.Bits.ReadWrite = 0;
      PagingAttrMask.Bits.Dirty      = 1;
      if (mIsShadowStack) {
        // Environment setup
        // ReadOnly page need set Dirty bit for shadow stack
        PagingAttribute.Bits.Dirty = 1;
        // Clear user bit for supervisor shadow stack
        PagingAttribute.Bits.UserSupervisor = 0;
        PagingAttrMask.Bits.UserSupervisor  = 1;
      } else {
        // Runtime update
        // Clear dirty bit for non shadow stack, to protect RO page.
        PagingAttribute.Bits.Dirty = 0;
      }
    } else {
      PagingAttribute.Bits.ReadWrite = 0;
    }
  }

  if ((Attributes & EFI_MEMORY_XP) != 0) {
    if (mXdSupported) {
      PagingAttribute.Bits.Nx = IsSet ? 1 : 0;
      PagingAttrMask.Bits.Nx  = 1;
    }
  }

  PageTableBufferSize = 0;
  Status              = PageTableMap (&PageTableBase, PagingMode, NULL, &PageTableBufferSize, BaseAddress, Length, &PagingAttribute, &PagingAttrMask, IsModified);
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_INFO, "SMM(%d): 0x%x bytes needed for converting page table\n", __LINE__, PageTableBufferSize));
    PageTableBuffer = AllocatePageTableMemory (EFI_SIZE_TO_PAGES (PageTableBufferSize));
    ASSERT (PageTableBuffer != NULL);
    Status = PageTableMap (&PageTableBase, PagingMode, PageTableBuffer, &PageTableBufferSize, BaseAddress, Length, &PagingAttribute, &PagingAttrMask, IsModified);
  }

  ASSERT (Status == RETURN_SUCCESS);
  ASSERT (PageTableBufferSize == 0);

  return RETURN_SUCCESS;
}

/**
  FlushTlb on current processor.

  @param[in,out] Buffer  Pointer to private data buffer.
**/
VOID
EFIAPI
FlushTlbOnCurrentProcessor (
  IN OUT VOID  *Buffer
  )
{
  CpuFlushTlb ();
}

/**
  FlushTlb for all processors.
**/
VOID
FlushTlbForAll (
  VOID
  )
{
  UINTN  Index;

  FlushTlbOnCurrentProcessor (NULL);

  for (Index = 0; Index < gSmst->NumberOfCpus; Index++) {
    if (Index != gSmst->CurrentlyExecutingCpu) {
      // Force to start up AP in blocking mode,
      SmmBlockingStartupThisAp (FlushTlbOnCurrentProcessor, Index, NULL);
      // Do not check return status, because AP might not be present in some corner cases.
    }
  }
}

/**
  This function sets the attributes for the memory region specified by BaseAddress and
  Length from their current attributes to the attributes specified by Attributes.

  @param[in]   PageTableBase    The page table base.
  @param[in]   PagingMode       The paging mode.
  @param[in]   BaseAddress      The physical address that is the start address of a memory region.
  @param[in]   Length           The size in bytes of the memory region.
  @param[in]   Attributes       The bit mask of attributes to set for the memory region.

  @retval EFI_SUCCESS           The attributes were set for the memory region.
  @retval EFI_ACCESS_DENIED     The attributes for the memory resource range specified by
                                BaseAddress and Length cannot be modified.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes specified an illegal combination of attributes that
                                cannot be set together.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to modify the attributes of
                                the memory resource range.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the memory
                                resource range specified by BaseAddress and Length.
                                The bit mask of attributes is not support for the memory resource
                                range specified by BaseAddress and Length.

**/
EFI_STATUS
SmmSetMemoryAttributesEx (
  IN  UINTN                 PageTableBase,
  IN  PAGING_MODE           PagingMode,
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length,
  IN  UINT64                Attributes
  )
{
  EFI_STATUS  Status;
  BOOLEAN     IsModified;

  Status = ConvertMemoryPageAttributes (PageTableBase, PagingMode, BaseAddress, Length, Attributes, TRUE, &IsModified);
  if (!EFI_ERROR (Status)) {
    if (IsModified) {
      //
      // Flush TLB as last step
      //
      FlushTlbForAll ();
    }
  }

  return Status;
}

/**
  This function clears the attributes for the memory region specified by BaseAddress and
  Length from their current attributes to the attributes specified by Attributes.

  @param[in]   PageTableBase    The page table base.
  @param[in]   PagingMode       The paging mode.
  @param[in]   BaseAddress      The physical address that is the start address of a memory region.
  @param[in]   Length           The size in bytes of the memory region.
  @param[in]   Attributes       The bit mask of attributes to clear for the memory region.

  @retval EFI_SUCCESS           The attributes were cleared for the memory region.
  @retval EFI_ACCESS_DENIED     The attributes for the memory resource range specified by
                                BaseAddress and Length cannot be modified.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes specified an illegal combination of attributes that
                                cannot be cleared together.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to modify the attributes of
                                the memory resource range.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the memory
                                resource range specified by BaseAddress and Length.
                                The bit mask of attributes is not supported for the memory resource
                                range specified by BaseAddress and Length.

**/
EFI_STATUS
SmmClearMemoryAttributesEx (
  IN  UINTN                 PageTableBase,
  IN  PAGING_MODE           PagingMode,
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length,
  IN  UINT64                Attributes
  )
{
  EFI_STATUS  Status;
  BOOLEAN     IsModified;

  Status = ConvertMemoryPageAttributes (PageTableBase, PagingMode, BaseAddress, Length, Attributes, FALSE, &IsModified);
  if (!EFI_ERROR (Status)) {
    if (IsModified) {
      //
      // Flush TLB as last step
      //
      FlushTlbForAll ();
    }
  }

  return Status;
}

/**
  This function sets the attributes for the memory region specified by BaseAddress and
  Length from their current attributes to the attributes specified by Attributes.

  @param[in]  BaseAddress      The physical address that is the start address of a memory region.
  @param[in]  Length           The size in bytes of the memory region.
  @param[in]  Attributes       The bit mask of attributes to set for the memory region.

  @retval EFI_SUCCESS           The attributes were set for the memory region.
  @retval EFI_ACCESS_DENIED     The attributes for the memory resource range specified by
                                BaseAddress and Length cannot be modified.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes specified an illegal combination of attributes that
                                cannot be set together.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to modify the attributes of
                                the memory resource range.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the memory
                                resource range specified by BaseAddress and Length.
                                The bit mask of attributes is not supported for the memory resource
                                range specified by BaseAddress and Length.

**/
EFI_STATUS
SmmSetMemoryAttributes (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length,
  IN  UINT64                Attributes
  )
{
  UINTN  PageTableBase;

  PageTableBase = AsmReadCr3 () & PAGING_4K_ADDRESS_MASK_64;
  return SmmSetMemoryAttributesEx (PageTableBase, mPagingMode, BaseAddress, Length, Attributes);
}

/**
  This function clears the attributes for the memory region specified by BaseAddress and
  Length from their current attributes to the attributes specified by Attributes.

  @param[in]  BaseAddress      The physical address that is the start address of a memory region.
  @param[in]  Length           The size in bytes of the memory region.
  @param[in]  Attributes       The bit mask of attributes to clear for the memory region.

  @retval EFI_SUCCESS           The attributes were cleared for the memory region.
  @retval EFI_ACCESS_DENIED     The attributes for the memory resource range specified by
                                BaseAddress and Length cannot be modified.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes specified an illegal combination of attributes that
                                cannot be cleared together.
  @retval EFI_OUT_OF_RESOURCES  There are not enough system resources to modify the attributes of
                                the memory resource range.
  @retval EFI_UNSUPPORTED       The processor does not support one or more bytes of the memory
                                resource range specified by BaseAddress and Length.
                                The bit mask of attributes is not supported for the memory resource
                                range specified by BaseAddress and Length.

**/
EFI_STATUS
SmmClearMemoryAttributes (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length,
  IN  UINT64                Attributes
  )
{
  UINTN  PageTableBase;

  PageTableBase = AsmReadCr3 () & PAGING_4K_ADDRESS_MASK_64;
  return SmmClearMemoryAttributesEx (PageTableBase, mPagingMode, BaseAddress, Length, Attributes);
}

/**
  Set ShadowStack memory.

  @param[in]  Cr3              The page table base address.
  @param[in]  BaseAddress      The physical address that is the start address of a memory region.
  @param[in]  Length           The size in bytes of the memory region.

  @retval EFI_SUCCESS           The shadow stack memory is set.
**/
EFI_STATUS
SetShadowStack (
  IN  UINTN                 Cr3,
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length
  )
{
  EFI_STATUS  Status;

  mIsShadowStack = TRUE;
  Status         = SmmSetMemoryAttributesEx (Cr3, mPagingMode, BaseAddress, Length, EFI_MEMORY_RO);
  mIsShadowStack = FALSE;

  return Status;
}

/**
  Set not present memory.

  @param[in]  Cr3              The page table base address.
  @param[in]  BaseAddress      The physical address that is the start address of a memory region.
  @param[in]  Length           The size in bytes of the memory region.

  @retval EFI_SUCCESS           The not present memory is set.
**/
EFI_STATUS
SetNotPresentPage (
  IN  UINTN                 Cr3,
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length
  )
{
  EFI_STATUS  Status;

  Status = SmmSetMemoryAttributesEx (Cr3, mPagingMode, BaseAddress, Length, EFI_MEMORY_RP);
  return Status;
}

/**
  Retrieves a pointer to the system configuration table from the SMM System Table
  based on a specified GUID.

  @param[in]   TableGuid       The pointer to table's GUID type.
  @param[out]  Table           The pointer to the table associated with TableGuid in the EFI System Table.

  @retval EFI_SUCCESS     A configuration table matching TableGuid was found.
  @retval EFI_NOT_FOUND   A configuration table matching TableGuid could not be found.

**/
EFI_STATUS
EFIAPI
SmmGetSystemConfigurationTable (
  IN  EFI_GUID  *TableGuid,
  OUT VOID      **Table
  )
{
  UINTN  Index;

  ASSERT (TableGuid != NULL);
  ASSERT (Table != NULL);

  *Table = NULL;
  for (Index = 0; Index < gSmst->NumberOfTableEntries; Index++) {
    if (CompareGuid (TableGuid, &(gSmst->SmmConfigurationTable[Index].VendorGuid))) {
      *Table = gSmst->SmmConfigurationTable[Index].VendorTable;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  This function sets SMM save state buffer to be RW and XP.
**/
VOID
PatchSmmSaveStateMap (
  VOID
  )
{
  UINTN  Index;
  UINTN  TileCodeSize;
  UINTN  TileDataSize;
  UINTN  TileSize;

  TileCodeSize = GetSmiHandlerSize ();
  TileCodeSize = ALIGN_VALUE (TileCodeSize, SIZE_4KB);
  TileDataSize = (SMRAM_SAVE_STATE_MAP_OFFSET - SMM_PSD_OFFSET) + sizeof (SMRAM_SAVE_STATE_MAP);
  TileDataSize = ALIGN_VALUE (TileDataSize, SIZE_4KB);
  TileSize     = TileDataSize + TileCodeSize - 1;
  TileSize     = 2 * GetPowerOfTwo32 ((UINT32)TileSize);

  DEBUG ((DEBUG_INFO, "PatchSmmSaveStateMap:\n"));
  for (Index = 0; Index < mMaxNumberOfCpus - 1; Index++) {
    //
    // Code
    //
    SmmSetMemoryAttributes (
      mCpuHotPlugData.SmBase[Index] + SMM_HANDLER_OFFSET,
      TileCodeSize,
      EFI_MEMORY_RO
      );
    SmmClearMemoryAttributes (
      mCpuHotPlugData.SmBase[Index] + SMM_HANDLER_OFFSET,
      TileCodeSize,
      EFI_MEMORY_XP
      );

    //
    // Data
    //
    SmmClearMemoryAttributes (
      mCpuHotPlugData.SmBase[Index] + SMM_HANDLER_OFFSET + TileCodeSize,
      TileSize - TileCodeSize,
      EFI_MEMORY_RO
      );
    SmmSetMemoryAttributes (
      mCpuHotPlugData.SmBase[Index] + SMM_HANDLER_OFFSET + TileCodeSize,
      TileSize - TileCodeSize,
      EFI_MEMORY_XP
      );
  }

  //
  // Code
  //
  SmmSetMemoryAttributes (
    mCpuHotPlugData.SmBase[mMaxNumberOfCpus - 1] + SMM_HANDLER_OFFSET,
    TileCodeSize,
    EFI_MEMORY_RO
    );
  SmmClearMemoryAttributes (
    mCpuHotPlugData.SmBase[mMaxNumberOfCpus - 1] + SMM_HANDLER_OFFSET,
    TileCodeSize,
    EFI_MEMORY_XP
    );

  //
  // Data
  //
  SmmClearMemoryAttributes (
    mCpuHotPlugData.SmBase[mMaxNumberOfCpus - 1] + SMM_HANDLER_OFFSET + TileCodeSize,
    SIZE_32KB - TileCodeSize,
    EFI_MEMORY_RO
    );
  SmmSetMemoryAttributes (
    mCpuHotPlugData.SmBase[mMaxNumberOfCpus - 1] + SMM_HANDLER_OFFSET + TileCodeSize,
    SIZE_32KB - TileCodeSize,
    EFI_MEMORY_XP
    );
}

/**
  This function sets GDT/IDT buffer to be RO and XP.
**/
VOID
PatchGdtIdtMap (
  VOID
  )
{
  EFI_PHYSICAL_ADDRESS  BaseAddress;
  UINTN                 Size;

  //
  // GDT
  //
  DEBUG ((DEBUG_INFO, "PatchGdtIdtMap - GDT:\n"));

  BaseAddress = mGdtBuffer;
  Size        = ALIGN_VALUE (mGdtBufferSize, SIZE_4KB);
  //
  // The range should have been set to RO
  // if it is allocated with EfiRuntimeServicesCode.
  //
  SmmSetMemoryAttributes (
    BaseAddress,
    Size,
    EFI_MEMORY_XP
    );

  //
  // IDT
  //
  DEBUG ((DEBUG_INFO, "PatchGdtIdtMap - IDT:\n"));

  BaseAddress = gcSmiIdtr.Base;
  Size        = ALIGN_VALUE (gcSmiIdtr.Limit + 1, SIZE_4KB);
  //
  // The range should have been set to RO
  // if it is allocated with EfiRuntimeServicesCode.
  //
  SmmSetMemoryAttributes (
    BaseAddress,
    Size,
    EFI_MEMORY_XP
    );
}

/**
  This function sets memory attribute according to MemoryAttributesTable.
**/
VOID
SetMemMapAttributes (
  VOID
  )
{
  EFI_MEMORY_DESCRIPTOR                 *MemoryMap;
  EFI_MEMORY_DESCRIPTOR                 *MemoryMapStart;
  UINTN                                 MemoryMapEntryCount;
  UINTN                                 DescriptorSize;
  UINTN                                 Index;
  EDKII_PI_SMM_MEMORY_ATTRIBUTES_TABLE  *MemoryAttributesTable;

  SmmGetSystemConfigurationTable (&gEdkiiPiSmmMemoryAttributesTableGuid, (VOID **)&MemoryAttributesTable);
  if (MemoryAttributesTable == NULL) {
    DEBUG ((DEBUG_INFO, "MemoryAttributesTable - NULL\n"));
    return;
  }

  DEBUG ((DEBUG_INFO, "MemoryAttributesTable:\n"));
  DEBUG ((DEBUG_INFO, "  Version                   - 0x%08x\n", MemoryAttributesTable->Version));
  DEBUG ((DEBUG_INFO, "  NumberOfEntries           - 0x%08x\n", MemoryAttributesTable->NumberOfEntries));
  DEBUG ((DEBUG_INFO, "  DescriptorSize            - 0x%08x\n", MemoryAttributesTable->DescriptorSize));

  MemoryMapEntryCount = MemoryAttributesTable->NumberOfEntries;
  DescriptorSize      = MemoryAttributesTable->DescriptorSize;
  MemoryMapStart      = (EFI_MEMORY_DESCRIPTOR *)(MemoryAttributesTable + 1);
  MemoryMap           = MemoryMapStart;
  for (Index = 0; Index < MemoryMapEntryCount; Index++) {
    DEBUG ((DEBUG_INFO, "Entry (0x%x)\n", MemoryMap));
    DEBUG ((DEBUG_INFO, "  Type              - 0x%x\n", MemoryMap->Type));
    DEBUG ((DEBUG_INFO, "  PhysicalStart     - 0x%016lx\n", MemoryMap->PhysicalStart));
    DEBUG ((DEBUG_INFO, "  VirtualStart      - 0x%016lx\n", MemoryMap->VirtualStart));
    DEBUG ((DEBUG_INFO, "  NumberOfPages     - 0x%016lx\n", MemoryMap->NumberOfPages));
    DEBUG ((DEBUG_INFO, "  Attribute         - 0x%016lx\n", MemoryMap->Attribute));
    MemoryMap = NEXT_MEMORY_DESCRIPTOR (MemoryMap, DescriptorSize);
  }

  MemoryMap = MemoryMapStart;
  for (Index = 0; Index < MemoryMapEntryCount; Index++) {
    DEBUG ((DEBUG_VERBOSE, "SetAttribute: Memory Entry - 0x%lx, 0x%x\n", MemoryMap->PhysicalStart, MemoryMap->NumberOfPages));
    switch (MemoryMap->Type) {
      case EfiRuntimeServicesCode:
        SmmSetMemoryAttributes (
          MemoryMap->PhysicalStart,
          EFI_PAGES_TO_SIZE ((UINTN)MemoryMap->NumberOfPages),
          EFI_MEMORY_RO
          );
        break;
      case EfiRuntimeServicesData:
        SmmSetMemoryAttributes (
          MemoryMap->PhysicalStart,
          EFI_PAGES_TO_SIZE ((UINTN)MemoryMap->NumberOfPages),
          EFI_MEMORY_XP
          );
        break;
      default:
        SmmSetMemoryAttributes (
          MemoryMap->PhysicalStart,
          EFI_PAGES_TO_SIZE ((UINTN)MemoryMap->NumberOfPages),
          EFI_MEMORY_XP
          );
        break;
    }

    MemoryMap = NEXT_MEMORY_DESCRIPTOR (MemoryMap, DescriptorSize);
  }

  PatchSmmSaveStateMap ();
  PatchGdtIdtMap ();

  return;
}

/**
  Sort memory map entries based upon PhysicalStart, from low to high.

  @param  MemoryMap              A pointer to the buffer in which firmware places
                                 the current memory map.
  @param  MemoryMapSize          Size, in bytes, of the MemoryMap buffer.
  @param  DescriptorSize         Size, in bytes, of an individual EFI_MEMORY_DESCRIPTOR.
**/
STATIC
VOID
SortMemoryMap (
  IN OUT EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN UINTN                      MemoryMapSize,
  IN UINTN                      DescriptorSize
  )
{
  EFI_MEMORY_DESCRIPTOR  *MemoryMapEntry;
  EFI_MEMORY_DESCRIPTOR  *NextMemoryMapEntry;
  EFI_MEMORY_DESCRIPTOR  *MemoryMapEnd;
  EFI_MEMORY_DESCRIPTOR  TempMemoryMap;

  MemoryMapEntry     = MemoryMap;
  NextMemoryMapEntry = NEXT_MEMORY_DESCRIPTOR (MemoryMapEntry, DescriptorSize);
  MemoryMapEnd       = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + MemoryMapSize);
  while (MemoryMapEntry < MemoryMapEnd) {
    while (NextMemoryMapEntry < MemoryMapEnd) {
      if (MemoryMapEntry->PhysicalStart > NextMemoryMapEntry->PhysicalStart) {
        CopyMem (&TempMemoryMap, MemoryMapEntry, sizeof (EFI_MEMORY_DESCRIPTOR));
        CopyMem (MemoryMapEntry, NextMemoryMapEntry, sizeof (EFI_MEMORY_DESCRIPTOR));
        CopyMem (NextMemoryMapEntry, &TempMemoryMap, sizeof (EFI_MEMORY_DESCRIPTOR));
      }

      NextMemoryMapEntry = NEXT_MEMORY_DESCRIPTOR (NextMemoryMapEntry, DescriptorSize);
    }

    MemoryMapEntry     = NEXT_MEMORY_DESCRIPTOR (MemoryMapEntry, DescriptorSize);
    NextMemoryMapEntry = NEXT_MEMORY_DESCRIPTOR (MemoryMapEntry, DescriptorSize);
  }
}

/**
  Return if a UEFI memory page should be marked as not present in SMM page table.
  If the memory map entries type is
  EfiLoaderCode/Data, EfiBootServicesCode/Data, EfiConventionalMemory,
  EfiUnusableMemory, EfiACPIReclaimMemory, return TRUE.
  Or return FALSE.

  @param[in]  MemoryMap              A pointer to the memory descriptor.

  @return TRUE  The memory described will be marked as not present in SMM page table.
  @return FALSE The memory described will not be marked as not present in SMM page table.
**/
BOOLEAN
IsUefiPageNotPresent (
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap
  )
{
  switch (MemoryMap->Type) {
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:
    case EfiConventionalMemory:
    case EfiUnusableMemory:
    case EfiACPIReclaimMemory:
      return TRUE;
    default:
      return FALSE;
  }
}

/**
  Merge continuous memory map entries whose type is
  EfiLoaderCode/Data, EfiBootServicesCode/Data, EfiConventionalMemory,
  EfiUnusableMemory, EfiACPIReclaimMemory, because the memory described by
  these entries will be set as NOT present in SMM page table.

  @param[in, out]  MemoryMap              A pointer to the buffer in which firmware places
                                          the current memory map.
  @param[in, out]  MemoryMapSize          A pointer to the size, in bytes, of the
                                          MemoryMap buffer. On input, this is the size of
                                          the current memory map.  On output,
                                          it is the size of new memory map after merge.
  @param[in]       DescriptorSize         Size, in bytes, of an individual EFI_MEMORY_DESCRIPTOR.
**/
STATIC
VOID
MergeMemoryMapForNotPresentEntry (
  IN OUT EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN OUT UINTN                  *MemoryMapSize,
  IN UINTN                      DescriptorSize
  )
{
  EFI_MEMORY_DESCRIPTOR  *MemoryMapEntry;
  EFI_MEMORY_DESCRIPTOR  *MemoryMapEnd;
  UINT64                 MemoryBlockLength;
  EFI_MEMORY_DESCRIPTOR  *NewMemoryMapEntry;
  EFI_MEMORY_DESCRIPTOR  *NextMemoryMapEntry;

  MemoryMapEntry    = MemoryMap;
  NewMemoryMapEntry = MemoryMap;
  MemoryMapEnd      = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + *MemoryMapSize);
  while ((UINTN)MemoryMapEntry < (UINTN)MemoryMapEnd) {
    CopyMem (NewMemoryMapEntry, MemoryMapEntry, sizeof (EFI_MEMORY_DESCRIPTOR));
    NextMemoryMapEntry = NEXT_MEMORY_DESCRIPTOR (MemoryMapEntry, DescriptorSize);

    do {
      MemoryBlockLength = (UINT64)(EFI_PAGES_TO_SIZE ((UINTN)MemoryMapEntry->NumberOfPages));
      if (((UINTN)NextMemoryMapEntry < (UINTN)MemoryMapEnd) &&
          IsUefiPageNotPresent (MemoryMapEntry) && IsUefiPageNotPresent (NextMemoryMapEntry) &&
          ((MemoryMapEntry->PhysicalStart + MemoryBlockLength) == NextMemoryMapEntry->PhysicalStart))
      {
        MemoryMapEntry->NumberOfPages += NextMemoryMapEntry->NumberOfPages;
        if (NewMemoryMapEntry != MemoryMapEntry) {
          NewMemoryMapEntry->NumberOfPages += NextMemoryMapEntry->NumberOfPages;
        }

        NextMemoryMapEntry = NEXT_MEMORY_DESCRIPTOR (NextMemoryMapEntry, DescriptorSize);
        continue;
      } else {
        MemoryMapEntry = PREVIOUS_MEMORY_DESCRIPTOR (NextMemoryMapEntry, DescriptorSize);
        break;
      }
    } while (TRUE);

    MemoryMapEntry    = NEXT_MEMORY_DESCRIPTOR (MemoryMapEntry, DescriptorSize);
    NewMemoryMapEntry = NEXT_MEMORY_DESCRIPTOR (NewMemoryMapEntry, DescriptorSize);
  }

  *MemoryMapSize = (UINTN)NewMemoryMapEntry - (UINTN)MemoryMap;

  return;
}

/**
  This function caches the GCD memory map information.
**/
VOID
GetGcdMemoryMap (
  VOID
  )
{
  UINTN                            NumberOfDescriptors;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *MemSpaceMap;
  EFI_STATUS                       Status;
  UINTN                            Index;

  Status = gDS->GetMemorySpaceMap (&NumberOfDescriptors, &MemSpaceMap);
  if (EFI_ERROR (Status)) {
    return;
  }

  mGcdMemNumberOfDesc = 0;
  for (Index = 0; Index < NumberOfDescriptors; Index++) {
    if ((MemSpaceMap[Index].GcdMemoryType == EfiGcdMemoryTypeReserved) &&
        ((MemSpaceMap[Index].Capabilities & (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED)) ==
         (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED))
        )
    {
      mGcdMemNumberOfDesc++;
    }
  }

  mGcdMemSpace = AllocateZeroPool (mGcdMemNumberOfDesc * sizeof (EFI_GCD_MEMORY_SPACE_DESCRIPTOR));
  ASSERT (mGcdMemSpace != NULL);
  if (mGcdMemSpace == NULL) {
    mGcdMemNumberOfDesc = 0;
    gBS->FreePool (MemSpaceMap);
    return;
  }

  mGcdMemNumberOfDesc = 0;
  for (Index = 0; Index < NumberOfDescriptors; Index++) {
    if ((MemSpaceMap[Index].GcdMemoryType == EfiGcdMemoryTypeReserved) &&
        ((MemSpaceMap[Index].Capabilities & (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED | EFI_MEMORY_TESTED)) ==
         (EFI_MEMORY_PRESENT | EFI_MEMORY_INITIALIZED))
        )
    {
      CopyMem (
        &mGcdMemSpace[mGcdMemNumberOfDesc],
        &MemSpaceMap[Index],
        sizeof (EFI_GCD_MEMORY_SPACE_DESCRIPTOR)
        );
      mGcdMemNumberOfDesc++;
    }
  }

  gBS->FreePool (MemSpaceMap);
}

/**
  Get UEFI MemoryAttributesTable.
**/
VOID
GetUefiMemoryAttributesTable (
  VOID
  )
{
  EFI_STATUS                   Status;
  EFI_MEMORY_ATTRIBUTES_TABLE  *MemoryAttributesTable;
  UINTN                        MemoryAttributesTableSize;

  Status = EfiGetSystemConfigurationTable (&gEfiMemoryAttributesTableGuid, (VOID **)&MemoryAttributesTable);
  if (!EFI_ERROR (Status) && (MemoryAttributesTable != NULL)) {
    MemoryAttributesTableSize  = sizeof (EFI_MEMORY_ATTRIBUTES_TABLE) + MemoryAttributesTable->DescriptorSize * MemoryAttributesTable->NumberOfEntries;
    mUefiMemoryAttributesTable = AllocateCopyPool (MemoryAttributesTableSize, MemoryAttributesTable);
    ASSERT (mUefiMemoryAttributesTable != NULL);
  }
}

/**
  This function caches the UEFI memory map information.
**/
VOID
GetUefiMemoryMap (
  VOID
  )
{
  EFI_STATUS             Status;
  UINTN                  MapKey;
  UINT32                 DescriptorVersion;
  EFI_MEMORY_DESCRIPTOR  *MemoryMap;
  UINTN                  UefiMemoryMapSize;

  DEBUG ((DEBUG_INFO, "GetUefiMemoryMap\n"));

  UefiMemoryMapSize = 0;
  MemoryMap         = NULL;
  Status            = gBS->GetMemoryMap (
                             &UefiMemoryMapSize,
                             MemoryMap,
                             &MapKey,
                             &mUefiDescriptorSize,
                             &DescriptorVersion
                             );
  ASSERT (Status == EFI_BUFFER_TOO_SMALL);

  do {
    Status = gBS->AllocatePool (EfiBootServicesData, UefiMemoryMapSize, (VOID **)&MemoryMap);
    ASSERT (MemoryMap != NULL);
    if (MemoryMap == NULL) {
      return;
    }

    Status = gBS->GetMemoryMap (
                    &UefiMemoryMapSize,
                    MemoryMap,
                    &MapKey,
                    &mUefiDescriptorSize,
                    &DescriptorVersion
                    );
    if (EFI_ERROR (Status)) {
      gBS->FreePool (MemoryMap);
      MemoryMap = NULL;
    }
  } while (Status == EFI_BUFFER_TOO_SMALL);

  if (MemoryMap == NULL) {
    return;
  }

  SortMemoryMap (MemoryMap, UefiMemoryMapSize, mUefiDescriptorSize);
  MergeMemoryMapForNotPresentEntry (MemoryMap, &UefiMemoryMapSize, mUefiDescriptorSize);

  mUefiMemoryMapSize = UefiMemoryMapSize;
  mUefiMemoryMap     = AllocateCopyPool (UefiMemoryMapSize, MemoryMap);
  ASSERT (mUefiMemoryMap != NULL);

  gBS->FreePool (MemoryMap);

  //
  // Get additional information from GCD memory map.
  //
  GetGcdMemoryMap ();

  //
  // Get UEFI memory attributes table.
  //
  GetUefiMemoryAttributesTable ();
}

/**
  This function sets UEFI memory attribute according to UEFI memory map.

  The normal memory region is marked as not present, such as
  EfiLoaderCode/Data, EfiBootServicesCode/Data, EfiConventionalMemory,
  EfiUnusableMemory, EfiACPIReclaimMemory.
**/
VOID
SetUefiMemMapAttributes (
  VOID
  )
{
  EFI_STATUS             Status;
  EFI_MEMORY_DESCRIPTOR  *MemoryMap;
  UINTN                  MemoryMapEntryCount;
  UINTN                  Index;
  EFI_MEMORY_DESCRIPTOR  *Entry;

  DEBUG ((DEBUG_INFO, "SetUefiMemMapAttributes\n"));

  if (mUefiMemoryMap != NULL) {
    MemoryMapEntryCount = mUefiMemoryMapSize/mUefiDescriptorSize;
    MemoryMap           = mUefiMemoryMap;
    for (Index = 0; Index < MemoryMapEntryCount; Index++) {
      if (IsUefiPageNotPresent (MemoryMap)) {
        Status = SmmSetMemoryAttributes (
                   MemoryMap->PhysicalStart,
                   EFI_PAGES_TO_SIZE ((UINTN)MemoryMap->NumberOfPages),
                   EFI_MEMORY_RP
                   );
        DEBUG ((
          DEBUG_INFO,
          "UefiMemory protection: 0x%lx - 0x%lx %r\n",
          MemoryMap->PhysicalStart,
          MemoryMap->PhysicalStart + (UINT64)EFI_PAGES_TO_SIZE ((UINTN)MemoryMap->NumberOfPages),
          Status
          ));
      }

      MemoryMap = NEXT_MEMORY_DESCRIPTOR (MemoryMap, mUefiDescriptorSize);
    }
  }

  //
  // Do not free mUefiMemoryMap, it will be checked in IsSmmCommBufferForbiddenAddress().
  //

  //
  // Set untested memory as not present.
  //
  if (mGcdMemSpace != NULL) {
    for (Index = 0; Index < mGcdMemNumberOfDesc; Index++) {
      Status = SmmSetMemoryAttributes (
                 mGcdMemSpace[Index].BaseAddress,
                 mGcdMemSpace[Index].Length,
                 EFI_MEMORY_RP
                 );
      DEBUG ((
        DEBUG_INFO,
        "GcdMemory protection: 0x%lx - 0x%lx %r\n",
        mGcdMemSpace[Index].BaseAddress,
        mGcdMemSpace[Index].BaseAddress + mGcdMemSpace[Index].Length,
        Status
        ));
    }
  }

  //
  // Do not free mGcdMemSpace, it will be checked in IsSmmCommBufferForbiddenAddress().
  //

  //
  // Set UEFI runtime memory with EFI_MEMORY_RO as not present.
  //
  if (mUefiMemoryAttributesTable != NULL) {
    Entry = (EFI_MEMORY_DESCRIPTOR *)(mUefiMemoryAttributesTable + 1);
    for (Index = 0; Index < mUefiMemoryAttributesTable->NumberOfEntries; Index++) {
      if ((Entry->Type == EfiRuntimeServicesCode) || (Entry->Type == EfiRuntimeServicesData)) {
        if ((Entry->Attribute & EFI_MEMORY_RO) != 0) {
          Status = SmmSetMemoryAttributes (
                     Entry->PhysicalStart,
                     EFI_PAGES_TO_SIZE ((UINTN)Entry->NumberOfPages),
                     EFI_MEMORY_RP
                     );
          DEBUG ((
            DEBUG_INFO,
            "UefiMemoryAttribute protection: 0x%lx - 0x%lx %r\n",
            Entry->PhysicalStart,
            Entry->PhysicalStart + (UINT64)EFI_PAGES_TO_SIZE ((UINTN)Entry->NumberOfPages),
            Status
            ));
        }
      }

      Entry = NEXT_MEMORY_DESCRIPTOR (Entry, mUefiMemoryAttributesTable->DescriptorSize);
    }
  }

  //
  // Do not free mUefiMemoryAttributesTable, it will be checked in IsSmmCommBufferForbiddenAddress().
  //
}

/**
  Return if the Address is forbidden as SMM communication buffer.

  @param[in] Address the address to be checked

  @return TRUE  The address is forbidden as SMM communication buffer.
  @return FALSE The address is allowed as SMM communication buffer.
**/
BOOLEAN
IsSmmCommBufferForbiddenAddress (
  IN UINT64  Address
  )
{
  EFI_MEMORY_DESCRIPTOR  *MemoryMap;
  UINTN                  MemoryMapEntryCount;
  UINTN                  Index;
  EFI_MEMORY_DESCRIPTOR  *Entry;

  if (mUefiMemoryMap != NULL) {
    MemoryMap           = mUefiMemoryMap;
    MemoryMapEntryCount = mUefiMemoryMapSize/mUefiDescriptorSize;
    for (Index = 0; Index < MemoryMapEntryCount; Index++) {
      if (IsUefiPageNotPresent (MemoryMap)) {
        if ((Address >= MemoryMap->PhysicalStart) &&
            (Address < MemoryMap->PhysicalStart + EFI_PAGES_TO_SIZE ((UINTN)MemoryMap->NumberOfPages)))
        {
          return TRUE;
        }
      }

      MemoryMap = NEXT_MEMORY_DESCRIPTOR (MemoryMap, mUefiDescriptorSize);
    }
  }

  if (mGcdMemSpace != NULL) {
    for (Index = 0; Index < mGcdMemNumberOfDesc; Index++) {
      if ((Address >= mGcdMemSpace[Index].BaseAddress) &&
          (Address < mGcdMemSpace[Index].BaseAddress + mGcdMemSpace[Index].Length))
      {
        return TRUE;
      }
    }
  }

  if (mUefiMemoryAttributesTable != NULL) {
    Entry = (EFI_MEMORY_DESCRIPTOR *)(mUefiMemoryAttributesTable + 1);
    for (Index = 0; Index < mUefiMemoryAttributesTable->NumberOfEntries; Index++) {
      if ((Entry->Type == EfiRuntimeServicesCode) || (Entry->Type == EfiRuntimeServicesData)) {
        if ((Entry->Attribute & EFI_MEMORY_RO) != 0) {
          if ((Address >= Entry->PhysicalStart) &&
              (Address < Entry->PhysicalStart + LShiftU64 (Entry->NumberOfPages, EFI_PAGE_SHIFT)))
          {
            return TRUE;
          }

          Entry = NEXT_MEMORY_DESCRIPTOR (Entry, mUefiMemoryAttributesTable->DescriptorSize);
        }
      }
    }
  }

  return FALSE;
}

/**
  This function set given attributes of the memory region specified by
  BaseAddress and Length.

  @param  This              The EDKII_SMM_MEMORY_ATTRIBUTE_PROTOCOL instance.
  @param  BaseAddress       The physical address that is the start address of
                            a memory region.
  @param  Length            The size in bytes of the memory region.
  @param  Attributes        The bit mask of attributes to set for the memory
                            region.

  @retval EFI_SUCCESS           The attributes were set for the memory region.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes specified an illegal combination of
                                attributes that cannot be set together.
  @retval EFI_UNSUPPORTED       The processor does not support one or more
                                bytes of the memory resource range specified
                                by BaseAddress and Length.
                                The bit mask of attributes is not supported for
                                the memory resource range specified by
                                BaseAddress and Length.

**/
EFI_STATUS
EFIAPI
EdkiiSmmSetMemoryAttributes (
  IN  EDKII_SMM_MEMORY_ATTRIBUTE_PROTOCOL  *This,
  IN  EFI_PHYSICAL_ADDRESS                 BaseAddress,
  IN  UINT64                               Length,
  IN  UINT64                               Attributes
  )
{
  return SmmSetMemoryAttributes (BaseAddress, Length, Attributes);
}

/**
  This function clears given attributes of the memory region specified by
  BaseAddress and Length.

  @param  This              The EDKII_SMM_MEMORY_ATTRIBUTE_PROTOCOL instance.
  @param  BaseAddress       The physical address that is the start address of
                            a memory region.
  @param  Length            The size in bytes of the memory region.
  @param  Attributes        The bit mask of attributes to clear for the memory
                            region.

  @retval EFI_SUCCESS           The attributes were cleared for the memory region.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes specified an illegal combination of
                                attributes that cannot be cleared together.
  @retval EFI_UNSUPPORTED       The processor does not support one or more
                                bytes of the memory resource range specified
                                by BaseAddress and Length.
                                The bit mask of attributes is not supported for
                                the memory resource range specified by
                                BaseAddress and Length.

**/
EFI_STATUS
EFIAPI
EdkiiSmmClearMemoryAttributes (
  IN  EDKII_SMM_MEMORY_ATTRIBUTE_PROTOCOL  *This,
  IN  EFI_PHYSICAL_ADDRESS                 BaseAddress,
  IN  UINT64                               Length,
  IN  UINT64                               Attributes
  )
{
  return SmmClearMemoryAttributes (BaseAddress, Length, Attributes);
}

/**
  Create PageTable in SMRAM.

  @param[in]      Is32BitPageTable     Whether the page table is 32-bit PAE
  @param[in]      PhysicalAddressBits  The bits of physical address to map.
  @return         PageTable Address

**/
UINTN
GenSmmPageTable (
  IN PAGING_MODE  PagingMode,
  IN UINT8        PhysicalAddressBits
  )
{
  UINTN               PageTableBufferSize;
  UINTN               PageTable;
  VOID                *PageTableBuffer;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  UINTN               GuardPage;
  UINTN               Length;
  UINT8               PhysicalAddressBits;
  UINT8               MaxLevel;
  UINT8               MaximumSupportAddressBit;

  MaxLevel = (UINT8)(PagingMode >> 8);
  ASSERT ((MaxLevel >= 3) && (MaxLevel <= 5));

  MaximumSupportAddressBit = (MaxLevel == 3) ? 32 : MIN ((12 + MaxLevel * 9), 52);
  ASSERT (PhysicalAddressBits <= MaximumSupportAddressBit);

  Length              = LShiftU64 (1, PhysicalAddressBits);
  PageTable           = 0;
  MapAttribute.Uint64 = mAddressEncMask | ((MaxLevel == 3) ? IA32_PAE_PDPTE_ATTRIBUTE_BITS : PAGE_ATTRIBUTE_BITS);
  MapMask.Uint64      = ~0ull;
  PageTableBufferSize = 0;

  Status = PageTableMap (&PageTable, PagingMode, NULL, &PageTableBufferSize, 0, Length, &MapAttribute, &MapMask, NULL);
  ASSERT (Status == RETURN_BUFFER_TOO_SMALL);
  DEBUG ((DEBUG_INFO, "SMM(%d): 0x%x bytes needed for initial SMM page table\n", __LINE__, PageTableBufferSize));
  PageTableBuffer = AllocatePageTableMemory (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  ASSERT (PageTableBuffer != NULL);
  Status = PageTableMap (&PageTable, PagingMode, PageTableBuffer, &PageTableBufferSize, 0, Length, &MapAttribute, &MapMask, NULL);
  ASSERT (Status == RETURN_SUCCESS);
  ASSERT (PageTableBufferSize == 0);

  if (FeaturePcdGet (PcdCpuSmmStackGuard)) {
    for (int Index = 0; Index < gSmmCpuPrivate->SmmCoreEntryContext.NumberOfCpus; Index++) {
      GuardPage = mSmmStackArrayBase + EFI_PAGE_SIZE + Index * (mSmmStackSize + mSmmShadowStackSize);
      Status    = ConvertMemoryPageAttributes (PageTable, PagingMode, GuardPage, SIZE_4KB, EFI_MEMORY_RP, TRUE, NULL);
    }
  }

  if ((PcdGet8 (PcdNullPointerDetectionPropertyMask) & BIT1) != 0) {
    Status = ConvertMemoryPageAttributes (PageTable, PagingMode, 0, SIZE_4KB, EFI_MEMORY_RP, TRUE, NULL);
  }

  return (UINTN)PageTable;
}

/**
  This function retrieves the attributes of the memory region specified by
  BaseAddress and Length. If different attributes are got from different part
  of the memory region, EFI_NO_MAPPING will be returned.

  @param  This              The EDKII_SMM_MEMORY_ATTRIBUTE_PROTOCOL instance.
  @param  BaseAddress       The physical address that is the start address of
                            a memory region.
  @param  Length            The size in bytes of the memory region.
  @param  Attributes        Pointer to attributes returned.

  @retval EFI_SUCCESS           The attributes got for the memory region.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes is NULL.
  @retval EFI_NO_MAPPING        Attributes are not consistent cross the memory
                                region.
  @retval EFI_UNSUPPORTED       The processor does not support one or more
                                bytes of the memory resource range specified
                                by BaseAddress and Length.

**/
EFI_STATUS
EFIAPI
EdkiiSmmGetMemoryAttributes (
  IN  EDKII_SMM_MEMORY_ATTRIBUTE_PROTOCOL  *This,
  IN  EFI_PHYSICAL_ADDRESS                 BaseAddress,
  IN  UINT64                               Length,
  OUT UINT64                               *Attributes
  )
{
  EFI_PHYSICAL_ADDRESS  Address;
  UINT64                *PageEntry;
  UINT64                MemAttr;
  PAGE_ATTRIBUTE        PageAttr;
  INT64                 Size;
  UINTN                 PageTableBase;
  BOOLEAN               EnablePML5Paging;
  IA32_CR4              Cr4;

  if ((Length < SIZE_4KB) || (Attributes == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Size    = (INT64)Length;
  MemAttr = (UINT64)-1;

  PageTableBase    = AsmReadCr3 () & PAGING_4K_ADDRESS_MASK_64;
  Cr4.UintN        = AsmReadCr4 ();
  EnablePML5Paging = (BOOLEAN)(Cr4.Bits.LA57 == 1);

  do {
    PageEntry = GetPageTableEntry (PageTableBase, EnablePML5Paging, BaseAddress, &PageAttr);
    if ((PageEntry == NULL) || (PageAttr == PageNone)) {
      return EFI_UNSUPPORTED;
    }

    //
    // If the memory range is cross page table boundary, make sure they
    // share the same attribute. Return EFI_NO_MAPPING if not.
    //
    *Attributes = GetAttributesFromPageEntry (PageEntry);
    if ((MemAttr != (UINT64)-1) && (*Attributes != MemAttr)) {
      return EFI_NO_MAPPING;
    }

    switch (PageAttr) {
      case Page4K:
        Address      = *PageEntry & ~mAddressEncMask & PAGING_4K_ADDRESS_MASK_64;
        Size        -= (SIZE_4KB - (BaseAddress - Address));
        BaseAddress += (SIZE_4KB - (BaseAddress - Address));
        break;

      case Page2M:
        Address      = *PageEntry & ~mAddressEncMask & PAGING_2M_ADDRESS_MASK_64;
        Size        -= SIZE_2MB - (BaseAddress - Address);
        BaseAddress += SIZE_2MB - (BaseAddress - Address);
        break;

      case Page1G:
        Address      = *PageEntry & ~mAddressEncMask & PAGING_1G_ADDRESS_MASK_64;
        Size        -= SIZE_1GB - (BaseAddress - Address);
        BaseAddress += SIZE_1GB - (BaseAddress - Address);
        break;

      default:
        return EFI_UNSUPPORTED;
    }

    MemAttr = *Attributes;
  } while (Size > 0);

  return EFI_SUCCESS;
}

/**
  Prevent the memory pages used for SMM page table from been overwritten.
**/
VOID
EnablePageTableProtection (
  VOID
  )
{
  PAGE_TABLE_POOL       *HeadPool;
  PAGE_TABLE_POOL       *Pool;
  UINT64                PoolSize;
  EFI_PHYSICAL_ADDRESS  Address;

  if (mPageTablePool == NULL) {
    return;
  }

  AsmWriteCr0 (AsmReadCr0 () & ~CR0_WP);
  //
  // SmmSetMemoryAttributes might update mPageTablePool. It's safer to
  // remember original one in advance.
  //
  HeadPool = mPageTablePool;
  Pool     = HeadPool;
  do {
    Address  = (EFI_PHYSICAL_ADDRESS)(UINTN)Pool;
    PoolSize = Pool->Offset + EFI_PAGES_TO_SIZE (Pool->FreePages);

    //
    // The size of one pool must be multiple of PAGE_TABLE_POOL_UNIT_SIZE.
    // Let's apply the protection to them one by one.
    //
    SmmSetMemoryAttributes (Address, PoolSize, EFI_MEMORY_RO);
    Pool = Pool->NextPool;
  } while (Pool != HeadPool);

  //
  // Enable write protection, after page table attribute updated.
  //
  AsmWriteCr0 (AsmReadCr0 () | CR0_WP);
}

/**
  This function sets memory attribute for page table.
**/
VOID
SetPageTableAttributes (
  VOID
  )
{
  BOOLEAN  CetEnabled;

  if (!IfSetPageTableReadOnly ()) {
    return;
  }

  DEBUG ((DEBUG_INFO, "SetPageTableAttributes\n"));

  //
  // Disable write protection, because we need mark page table to be write protected.
  // We need *write* page table memory, to mark itself to be *read only*.
  //
  CetEnabled = ((AsmReadCr4 () & CR4_CET_ENABLE) != 0) ? TRUE : FALSE;
  if (CetEnabled) {
    //
    // CET must be disabled if WP is disabled.
    //
    DisableCet ();
  }

  // Set memory used by page table as Read Only.
  DEBUG ((DEBUG_INFO, "Start...\n"));
  EnablePageTableProtection ();

  if (CetEnabled) {
    //
    // re-enable CET.
    //
    EnableCet ();
  }

  return;
}
