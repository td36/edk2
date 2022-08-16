/** @file
  Unit tests of the CpuPageTableLib instance of the CpuPageTableLib class

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuPageTableLibUnitTest.h"

// ----------------------------------------------------------------------- PageMode--TestCount-TestRangeCount---RandomOptions
static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPaging4Level    = { Paging4Level, 100, 20, MANUAL_CHANGE_PAGE_TABLE|ONLY_ONE_ONE_MAPPING|USE_RANDOM_ARRAY|TEST_REMAP_WRITEABLE };
static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPaging4Level1GB = { Paging4Level1GB, 100, 20, MANUAL_CHANGE_PAGE_TABLE|ONLY_ONE_ONE_MAPPING|USE_RANDOM_ARRAY|TEST_REMAP_WRITEABLE };
static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPaging5Level    = { Paging5Level, 100, 20, MANUAL_CHANGE_PAGE_TABLE|ONLY_ONE_ONE_MAPPING|USE_RANDOM_ARRAY |TEST_REMAP_WRITEABLE };
static CPU_PAGE_TABLE_LIB_RANDOM_TEST_CONTEXT  mTestContextPaging5Level1GB = { Paging5Level1GB, 100, 20, MANUAL_CHANGE_PAGE_TABLE|ONLY_ONE_ONE_MAPPING|USE_RANDOM_ARRAY |TEST_REMAP_WRITEABLE };

/**
  Check if the input parameters are not supported.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseForParameter (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  PAGING_MODE         PagingMode;
  UINTN               Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;

  MapAttribute.Uint64 = 0;
  MapMask.Uint64      = 0;
  PagingMode          = Paging5Level1GB;
  PageTableBufferSize = 0;
  PageTable           = 0;

  //
  // If the input linear address is not 4K align, it should return invalid parameter
  //
  UT_ASSERT_EQUAL (PageTableMap (&PageTable, PagingMode, &Buffer, &PageTableBufferSize, 1, SIZE_4KB, &MapAttribute, &MapMask), RETURN_INVALID_PARAMETER);

  //
  // If the input PageTableBufferSize is not 4K align, it should return invalid parameter
  //
  PageTableBufferSize = 10;
  UT_ASSERT_EQUAL (PageTableMap (&PageTable, PagingMode, &Buffer, &PageTableBufferSize, 0, SIZE_4KB, &MapAttribute, &MapMask), RETURN_INVALID_PARAMETER);

  //
  // If the input PagingMode is Paging32bit, it should return invalid parameter
  //
  PageTableBufferSize = 0;
  PagingMode          = Paging32bit;
  UT_ASSERT_EQUAL (PageTableMap (&PageTable, PagingMode, &Buffer, &PageTableBufferSize, 1, SIZE_4KB, &MapAttribute, &MapMask), RETURN_UNSUPPORTED);

  //
  // If the input MapMask is NULL, it should return invalid parameter
  //
  PagingMode = Paging5Level1GB;
  UT_ASSERT_EQUAL (PageTableMap (&PageTable, PagingMode, &Buffer, &PageTableBufferSize, 1, SIZE_4KB, &MapAttribute, NULL), RETURN_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Check the case that modifying page table doesn't need extra buffe

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseWhichNoNeedExtraSize (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  UNIT_TEST_STATUS    TestStatus;

  MapAttribute.Uint64       = 0;
  MapMask.Uint64            = 0;
  PagingMode                = Paging4Level1GB;
  PageTableBufferSize       = 0;
  PageTable                 = 0;
  Buffer                    = NULL;
  MapAttribute.Bits.Present = 1;
  MapAttribute.Bits.Nx      = 1;
  MapMask.Bits.Present      = 1;
  MapMask.Uint64            = MAX_UINT64;

  //
  // Create page table to cover [0, 10M], it should have 5 PTE
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, 0, (UINT64)SIZE_2MB * 5, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, 0, (UINT64)SIZE_2MB * 5, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  //
  // call library to cover [0, 4K], because the page table is already cover [0, 10M], and no attribute change,
  // We assume the fucntion doesn't need to change page table, return success and output BufferSize is 0
  //
  Buffer = NULL;
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, 0, (UINT64)SIZE_4KB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (PageTableBufferSize, 0);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  //
  // Same range and same attribute, only clear one mask attribute bit
  // We assume the fucntion doesn't need to change page table, return success and output BufferSize is 0
  //
  MapMask.Bits.Nx     = 0;
  PageTableBufferSize = 0;
  Status              = PageTableMap (&PageTable, PagingMode, NULL, &PageTableBufferSize, 0, (UINT64)SIZE_4KB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (PageTableBufferSize, 0);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  //
  // call library to cover [2M, 4M], while the page table is already cover [0, 10M],
  // only change one attribute bit, we assume the page table change be modified even if the
  // input Buffer is NULL, and BufferSize is 0
  //
  MapAttribute.Bits.Accessed = 1;
  MapMask.Bits.Accessed      = 1;
  PageTableBufferSize        = 0;
  Status                     = PageTableMap (&PageTable, PagingMode, NULL, &PageTableBufferSize, (UINT64)SIZE_2MB, (UINT64)SIZE_2MB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (PageTableBufferSize, 0);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  return UNIT_TEST_PASSED;
}

/**
  Test Case that check the case to map [0, 1G] to [8K, 1G+8K]

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCase1Gmapto4K (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  UNIT_TEST_STATUS    TestStatus;

  //
  // Create Page table to map [0,1G] to [8K, 1G+8K]
  //
  PagingMode                = Paging4Level1GB;
  PageTableBufferSize       = 0;
  PageTable                 = 0;
  Buffer                    = NULL;
  MapAttribute.Uint64       = (UINT64)SIZE_4KB * 2;
  MapMask.Uint64            = (UINT64)SIZE_4KB * 2;
  MapAttribute.Bits.Present = 1;
  MapMask.Bits.Present      = 1;
  MapMask.Uint64            = MAX_UINT64;
  Status                    = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_1GB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_1GB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  //
  // Page table should be valid. (All reserved bits are zero)
  //
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  return UNIT_TEST_PASSED;
}

/**
  Check if the parent entry has different R/W attribute

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseManualChangeReadWrite (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  ExpectedMapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      *Map;
  UINTN               MapCount;
  IA32_PAGING_ENTRY   *PagingEntry;
  VOID                *BackupBuffer;
  UINTN               BackupPageTableBufferSize;

  PagingMode                  = Paging4Level;
  PageTableBufferSize         = 0;
  PageTable                   = 0;
  Buffer                      = NULL;
  MapAttribute.Uint64         = 0;
  MapMask.Uint64              = MAX_UINT64;
  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 1;

  //
  // Create Page table to cover [0,2G], with ReadWrite = 1
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, 0, SIZE_2GB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  BackupPageTableBufferSize = PageTableBufferSize;
  Buffer                    = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status                    = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, 0, SIZE_2GB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  IsPageTableValid (PageTable, PagingMode);

  MapCount = 0;
  Status   = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  IsPageTableValid (PageTable, PagingMode);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_2GB);
  ExpectedMapAttribute.Uint64 = MapAttribute.Uint64;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  //
  // Manually change ReadWrite to 0 for non-leaf entry, which covers [0,2G]
  //
  PagingEntry         = (IA32_PAGING_ENTRY *)(UINTN)PageTable;
  PagingEntry->Uint64 = PagingEntry->Uint64 & (~(UINT64)0x2);
  MapCount            = 0;
  Status              = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_2GB);
  ExpectedMapAttribute.Uint64         = MapAttribute.Uint64;
  ExpectedMapAttribute.Bits.ReadWrite = 0;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  //
  // Copy the page entry structure memory for future compare
  //
  BackupBuffer = AllocateCopyPool (BackupPageTableBufferSize, Buffer);
  UT_ASSERT_MEM_EQUAL (Buffer, BackupBuffer, BackupPageTableBufferSize);

  //
  // Call library to change ReadWrite to 0 for [0,2M]
  //
  MapAttribute.Bits.ReadWrite = 0;
  Status                      = PageTableMap (&PageTable, PagingMode, NULL, &PageTableBufferSize, 0, SIZE_2MB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  IsPageTableValid (PageTable, PagingMode);
  MapCount = 0;
  Status   = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);
  //
  // There should be 1 range [0, 2G] with ReadWrite = 0
  //
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_2GB);
  ExpectedMapAttribute.Uint64 = MapAttribute.Uint64;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  //
  // The latest PageTableMap call should change nothing.
  // The memory should be identical before and after the funtion is called.
  //
  UT_ASSERT_MEM_EQUAL (Buffer, BackupBuffer, BackupPageTableBufferSize);

  //
  // Call library to change ReadWrite to 1 for [0, 2M]
  //
  MapAttribute.Bits.ReadWrite = 1;
  PageTableBufferSize         = 0;
  Status                      = PageTableMap (&PageTable, PagingMode, NULL, &PageTableBufferSize, 0, SIZE_2MB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  IsPageTableValid (PageTable, PagingMode);
  MapCount = 0;
  Status   = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);
  //
  // There should be 2 range [0, 2M] with ReadWrite = 1 and [2M, 2G] with ReadWrite = 0
  //
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (MapCount, 2);

  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_2MB);
  ExpectedMapAttribute.Uint64 = MapAttribute.Uint64;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  UT_ASSERT_EQUAL (Map[1].LinearAddress, SIZE_2MB);
  UT_ASSERT_EQUAL (Map[1].Length, SIZE_2GB - SIZE_2MB);
  ExpectedMapAttribute.Uint64         = SIZE_2MB;
  ExpectedMapAttribute.Bits.ReadWrite = 0;
  ExpectedMapAttribute.Bits.Present   = 1;
  UT_ASSERT_EQUAL (Map[1].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  return UNIT_TEST_PASSED;
}

/**
  Check if the needed size is expected

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseManualSizeNotMatch (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  ExpectedMapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      *Map;
  UINTN               MapCount;
  IA32_PAGING_ENTRY   *PagingEntry;

  PagingMode                             = Paging4Level;
  PageTableBufferSize                    = 0;
  PageTable                              = 0;
  Buffer                                 = NULL;
  MapAttribute.Uint64                    = 0;
  MapMask.Uint64                         = MAX_UINT64;
  MapAttribute.Bits.Present              = 1;
  MapAttribute.Bits.ReadWrite            = 1;
  MapAttribute.Bits.PageTableBaseAddress = (SIZE_2MB - SIZE_4KB) >> 12;
  //
  // Create Page table to cover [2M-4K, 4M], with ReadWrite = 1
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, SIZE_2MB - SIZE_4KB, SIZE_4KB + SIZE_2MB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, SIZE_2MB - SIZE_4KB, SIZE_4KB + SIZE_2MB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  IsPageTableValid (PageTable, PagingMode);

  MapCount = 0;
  Status   = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  IsPageTableValid (PageTable, PagingMode);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, SIZE_2MB - SIZE_4KB);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_4KB + SIZE_2MB);
  ExpectedMapAttribute.Uint64 = MapAttribute.Uint64;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  //
  // Manually change ReadWrite to 0 for 3 level non-leaf entry, which covers [0,2M]
  // Then the map is:
  // [2M-4K,2M], R/W = 0
  // [2M   ,4M], R/W = 1
  //
  PagingEntry         = (IA32_PAGING_ENTRY *)(UINTN)PageTable;                                           // Get 4 level entry
  PagingEntry         = (IA32_PAGING_ENTRY *)(UINTN)(PagingEntry->Pnle.Bits.PageTableBaseAddress << 12); // Get 3 level entry
  PagingEntry         = (IA32_PAGING_ENTRY *)(UINTN)(PagingEntry->Pnle.Bits.PageTableBaseAddress << 12); // Get 2 level entry
  PagingEntry->Uint64 = PagingEntry->Uint64 & (~(UINT64)0x2);
  MapCount            = 0;
  Status              = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (MapCount, 2);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, SIZE_2MB - SIZE_4KB);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_4KB);
  ExpectedMapAttribute.Uint64         = MapAttribute.Uint64;
  ExpectedMapAttribute.Bits.ReadWrite = 0;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  UT_ASSERT_EQUAL (Map[1].LinearAddress, SIZE_2MB);
  UT_ASSERT_EQUAL (Map[1].Length, SIZE_2MB);
  ExpectedMapAttribute.Uint64                    = MapAttribute.Uint64;
  ExpectedMapAttribute.Bits.ReadWrite            = 1;
  ExpectedMapAttribute.Bits.PageTableBaseAddress = SIZE_2MB >> 12;
  UT_ASSERT_EQUAL (Map[1].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  //
  // Set Page table [2M-4K, 2M+4K]'s ReadWrite = 1, [2M,2M+4K]'s ReadWrite is already 1
  // Just need to set [2M-4K,2M], won't need extra size, so the status should be success
  //
  MapAttribute.Bits.Present              = 1;
  MapAttribute.Bits.ReadWrite            = 1;
  PageTableBufferSize                    = 0;
  MapAttribute.Bits.PageTableBaseAddress = (SIZE_2MB - SIZE_4KB) >> 12;
  Status                                 = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, SIZE_2MB - SIZE_4KB, SIZE_4KB * 2, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  return UNIT_TEST_PASSED;
}

/**
  Check that won't merge entries

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseManualNotMergeEntry (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  UNIT_TEST_STATUS    TestStatus;

  PagingMode                = Paging4Level1GB;
  PageTableBufferSize       = 0;
  PageTable                 = 0;
  Buffer                    = NULL;
  MapAttribute.Uint64       = 0;
  MapMask.Uint64            = MAX_UINT64;
  MapAttribute.Bits.Present = 1;
  MapMask.Bits.Present      = 1;

  //
  // Create Page table to cover [0,4M], and [4M, 1G] is not present
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB * 2, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB * 2, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  //
  // Let Page table to cover [0,1G], we assume it won't use a big 1G entry to cover whole range
  // It looks like the chioce is not bad, but sometime, we need to keep some small entry
  //
  PageTableBufferSize = 0;
  Status              = PageTableMap (&PageTable, PagingMode, NULL, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_1GB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  MapAttribute.Bits.Accessed = 1;
  PageTableBufferSize        = 0;
  Status                     = PageTableMap (&PageTable, PagingMode, NULL, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB, &MapAttribute, &MapMask);
  //
  // If it didn't use a big 1G entry to cover whole range, only change [0,2M] for some attribute won't need extra memory
  //
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  Test case for PageTableRemapWritable

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseForPageTableRemapWritable (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  UINTN               NewPageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  UNIT_TEST_STATUS    TestStatus;
  IA32_MAP_ENTRY      *Map;
  UINTN               MapCount;

  PagingMode                  = Paging4Level1GB;
  PageTableBufferSize         = 0;
  PageTable                   = 0;
  Buffer                      = NULL;
  MapAttribute.Uint64         = 0;
  MapMask.Uint64              = MAX_UINT64;
  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 0;
  MapMask.Bits.Present        = 1;

  //
  // Create Page table to cover [0,4M] with ReadWrite bit as 0, and [4M, 1G] is not present
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB * 2, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB * 2, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  //
  // Remap pagetable [0,1G] with ReadWrite as 1
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, 0, (UINT64)SIZE_1GB);

  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);

  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableRemapWritable (&NewPageTable, 4, Buffer, &PageTableBufferSize, 0, (UINT64)SIZE_1GB);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  MapCount = 0;
  Status   = PageTableParse ((UINTN)NewPageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse ((UINTN)NewPageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, (UINT64)SIZE_2MB * 2);
  MapAttribute.Bits.ReadWrite = 1;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, MapAttribute.Uint64);

  //
  // Remap pagetable [512G,513G] with ReadWrite as 1, directly return success
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;
  Status              = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)SIZE_1GB *512, (UINT64)SIZE_1GB);

  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (PageTableBufferSize, 0);
  return UNIT_TEST_PASSED;
}

/**
  Test case for minimal size usage of PageTableRemapWritable

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseForSizeUsageOfPageTableRemapWritable (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  UINTN               NewPageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      *Map;
  UINTN               MapCount;
  IA32_PML4E          *Pml4;
  IA32_PDPTE          *Pdpte;
  IA32_PDE_2M         *Pde2M;

  PagingMode                  = Paging4Level1GB;
  PageTableBufferSize         = 0;
  PageTable                   = 0;
  Buffer                      = NULL;
  MapAttribute.Uint64         = (UINT64)SIZE_1GB - SIZE_2MB;
  MapMask.Uint64              = MAX_UINT64;
  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 0;
  MapMask.Bits.Present        = 1;

  //
  // Create Page table to cover [1G - 2M,2G] with ReadWrite bit as 0
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)SIZE_1GB - SIZE_2MB, (UINT64)SIZE_1GB + SIZE_2MB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)SIZE_1GB - SIZE_2MB, (UINT64)SIZE_1GB + SIZE_2MB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  //
  // Manual change [1G - 2M,1G] as not present
  //
  // Page table layout is as below:
  //
  // [IA32_CR3]
  //     |
  //     |
  //     V
  //  [IA32_PML4E] --> [IA32_PDPTE]
  //                     ...
  //                   [IA32_PDPTE] --> [IA32_PDE_2M]
  //                                        ...
  //                                    [IA32_PDE_2M]
  //
  Pml4  = (IA32_PML4E *)PageTable;
  Pdpte = (IA32_PDPTE *)IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (Pml4);
  Pde2M = (IA32_PDE_2M *)IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (Pdpte);
  UT_ASSERT_EQUAL ((UINT64)SIZE_1GB - SIZE_2MB, IA32_PLEB_PAGE_TABLE_BASE_ADDRESS (&Pde2M[511]));
  UT_ASSERT_EQUAL (1, Pde2M[511].Bits.Present);
  Pde2M[511].Bits.Present = 0;

  MapCount = 0;
  Status   = PageTableParse ((UINTN)PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse ((UINTN)PageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, SIZE_1GB);
  UT_ASSERT_EQUAL (Map[0].Length, (UINT64)SIZE_1GB);

  //
  // Remap pagetable [0 , 2G] with ReadWrite as 1
  // However, since [0 , 1G] is not present, only [1G, 2G] is mapped.
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2GB);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  UT_ASSERT_EQUAL (PageTableBufferSize, EFI_PAGES_TO_SIZE (2));

  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableRemapWritable (&NewPageTable, 4, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2GB);
  UT_ASSERT_NOT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  MapCount = 0;
  Status   = PageTableParse ((UINTN)NewPageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse ((UINTN)NewPageTable, PagingMode, Map, &MapCount);

  MapAttribute.Bits.ReadWrite            = 1;
  MapAttribute.Bits.PageTableBaseAddress = ((UINT64)SIZE_1GB) >> 12;
  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, SIZE_1GB);
  UT_ASSERT_EQUAL (Map[0].Length, (UINT64)SIZE_1GB);
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, MapAttribute.Uint64);

  PagingMode                  = Paging4Level1GB;
  PageTableBufferSize         = 0;
  PageTable                   = 0;
  Buffer                      = NULL;
  MapAttribute.Uint64         = 0;
  MapMask.Uint64              = MAX_UINT64;
  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 0;
  MapMask.Bits.Present        = 1;
  //
  // Create Page table to cover [0,10M] with ReadWrite bit as 0, and [10M, 1G] is not present
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB * 5, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB * 5, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  //
  // Manual change [2M , 4M] as not present
  //
  // Page table layout is as below:
  //
  // [IA32_CR3]
  //     |
  //     |
  //     V
  //  [IA32_PML4E] --> [IA32_PDPTE]
  //                     ...
  //                   [IA32_PDPTE] --> [IA32_PDE_2M]
  //                                        ...
  //                                    [IA32_PDE_2M]
  //
  Pml4  = (IA32_PML4E *)PageTable;
  Pdpte = (IA32_PDPTE *)IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (Pml4);
  Pde2M = (IA32_PDE_2M *)IA32_PNLE_PAGE_TABLE_BASE_ADDRESS (Pdpte);
  UT_ASSERT_EQUAL (SIZE_2MB, IA32_PLEB_PAGE_TABLE_BASE_ADDRESS (&Pde2M[1]));
  UT_ASSERT_EQUAL (1, Pde2M[1].Bits.Present);
  Pde2M[1].Bits.Present = 0;

  //
  // Check if [2M , 4M] is not present
  //
  MapCount = 0;
  Status   = PageTableParse ((UINTN)PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse ((UINTN)PageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (MapCount, 2);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, (UINT64)SIZE_2MB);
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, MapAttribute.Uint64);

  UT_ASSERT_EQUAL (Map[1].LinearAddress, (UINT64)SIZE_2MB*2);
  UT_ASSERT_EQUAL (Map[1].Length, (UINT64)SIZE_2MB*3);
  UT_ASSERT_EQUAL (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[1].Attribute), IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute));

  //
  // Remap pagetable [2M , 4M] with ReadWrite as 1
  // However, since [2M , 4M] is not present, BufferSize should be zero, and returned input page table itself
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)SIZE_2MB, (UINT64)SIZE_2MB);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (PageTableBufferSize, 0);

  //
  // Remap pagetable [10M , 2G] with ReadWrite as 1
  // However, since [10M , 2G] is not present, BufferSize should be zero, and returned input page table itself
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)SIZE_2MB*5, (UINT64)SIZE_2GB - SIZE_2MB*5);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (PageTableBufferSize, 0);

  //
  // Remap pagetable [8M , 2G] with ReadWrite as 1
  // However, since [8M , 10M] is present, need 3 pages, return buffer too small, and pagetable unchanged
  // Page table layout is as below:
  //
  //  [IA32_CR3*]
  //     |
  //     |
  //     V
  //  [IA32_PML4E]
  //  ...
  //  [IA32_PML4E*] --> [IA32_PDPTE]
  //                     ...
  //                   [IA32_PDPTE*] --> [IA32_PDE_2M]
  //                                        ...
  //                                    [IA32_PDE_2M]
  // The one marked with * will be created a new one. so need 4 pages
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)SIZE_2MB*4, (UINT64)SIZE_2GB - SIZE_2MB*4);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  UT_ASSERT_EQUAL (PageTableBufferSize, EFI_PAGES_TO_SIZE (3));

  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableRemapWritable (&NewPageTable, 4, Buffer, &PageTableBufferSize, (UINT64)SIZE_2MB*4, (UINT64)SIZE_2GB - SIZE_2MB*4);
  UT_ASSERT_NOT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  MapCount = 0;
  Status   = PageTableParse ((UINTN)NewPageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse ((UINTN)NewPageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (MapCount, 3);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, (UINT64)SIZE_2MB);
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, MapAttribute.Uint64);

  UT_ASSERT_EQUAL (Map[1].LinearAddress, (UINT64)SIZE_2MB*2);
  UT_ASSERT_EQUAL (Map[1].Length, (UINT64)SIZE_2MB*2);
  UT_ASSERT_EQUAL (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[1].Attribute), IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute));

  UT_ASSERT_EQUAL (Map[2].LinearAddress, (UINT64)SIZE_2MB*4);
  UT_ASSERT_EQUAL (Map[2].Length, (UINT64)SIZE_2MB);
  MapAttribute.Bits.ReadWrite = 1;
  UT_ASSERT_EQUAL (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[2].Attribute), IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute));

  return UNIT_TEST_PASSED;
}

/**
  Test case for minimal size usage of PageTableRemapWritable with 4K page entry

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseForSizeUsageOfPageTableRemapWritable4K (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  UINTN               NewPageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      *Map;
  UINTN               MapCount;

  PagingMode                  = Paging4Level1GB;
  PageTableBufferSize         = 0;
  PageTable                   = 0;
  Buffer                      = NULL;
  MapAttribute.Uint64         = 0;
  MapMask.Uint64              = MAX_UINT64;
  MapAttribute.Bits.Present   = 1;
  MapAttribute.Bits.ReadWrite = 0;
  MapMask.Bits.Present        = 1;

  //
  // Create Page table to cover [0,10M] with ReadWrite bit as 0, and [10M, 1G] is not present
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB * 5, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_2MB * 5, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  //
  // Mark  [2M,2M+4K]  not present
  //
  MapAttribute.Bits.Present = 0;
  Status                    = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)SIZE_2MB, (UINT64)SIZE_4KB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)SIZE_2MB, (UINT64)SIZE_4KB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  //
  // Check
  //
  MapAttribute.Bits.Present = 1;
  MapCount                  = 0;
  Status                    = PageTableParse ((UINTN)PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse ((UINTN)PageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (MapCount, 2);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, (UINT64)SIZE_2MB);
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, MapAttribute.Uint64);

  UT_ASSERT_EQUAL (Map[1].LinearAddress, (UINT64)SIZE_2MB + SIZE_4KB);
  UT_ASSERT_EQUAL (Map[1].Length, (UINT64)SIZE_2MB*4 - SIZE_4KB);
  UT_ASSERT_EQUAL (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[1].Attribute), IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute));

  //
  // Remap pagetable [2M,2M+4K]  with ReadWrite as 1
  // However, since [2M,2M+4K]  is not present, BufferSize should be zero, and returned input page table itself
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)SIZE_2MB, (UINT64)SIZE_4KB);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (PageTableBufferSize, 0);

  //
  // Remap pagetable [10M , 2G] with ReadWrite as 1
  // However, since [10M , 2G] is not present, BufferSize should be zero, and returned input page table itself
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)SIZE_2MB*5, (UINT64)SIZE_2GB - SIZE_2MB*5);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (PageTableBufferSize, 0);

  //
  // Remap pagetable [4M-4K , 2G + 4M - 4K] with ReadWrite as 1
  // However, since [4M-4K , 10M] is present, need 4 pages, return buffer too small, and pagetable unchanged
  //
  // Page table layout is as below:
  //
  //  [IA32_CR3*]
  //     |
  //     |
  //     V
  //  [IA32_PML4E]
  //  ...
  //  [IA32_PML4E*] --> [IA32_PDPTE]
  //                     ...
  //                   [IA32_PDPTE*] --> [IA32_PDE_2M]
  //                                        ...
  //                                    [IA32_PDE*]      --> [IA32_PTE_4K]  --> 4K aligned physical address
  //                                                        ...
  //                                                        [IA32_PTE_4K]  --> 4K aligned physical address
  //                                    [IA32_PDE_2M]
  // The one marked with * will be created a new one. so need 4 pages
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)SIZE_2MB*2 -SIZE_4KB, (UINT64)SIZE_2GB);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  UT_ASSERT_EQUAL (PageTableBufferSize, EFI_PAGES_TO_SIZE (4));

  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableRemapWritable (&NewPageTable, 4, Buffer, &PageTableBufferSize, (UINT64)SIZE_2MB*2 -SIZE_4KB, (UINT64)SIZE_2GB);
  UT_ASSERT_NOT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  MapCount = 0;
  Status   = PageTableParse ((UINTN)NewPageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse ((UINTN)NewPageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (MapCount, 3);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, (UINT64)SIZE_2MB);
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, MapAttribute.Uint64);

  UT_ASSERT_EQUAL (Map[1].LinearAddress, (UINT64)SIZE_2MB+SIZE_4KB);
  UT_ASSERT_EQUAL (Map[1].Length, (UINT64)SIZE_2MB -SIZE_4KB - SIZE_4KB);
  UT_ASSERT_EQUAL (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[1].Attribute), IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute));

  UT_ASSERT_EQUAL (Map[2].LinearAddress, (UINT64)SIZE_2MB*2- SIZE_4KB);
  UT_ASSERT_EQUAL (Map[2].Length, (UINT64)SIZE_2MB*3 + SIZE_4KB);
  MapAttribute.Bits.ReadWrite = 1;
  UT_ASSERT_EQUAL (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[2].Attribute), IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute));
  MapAttribute.Bits.ReadWrite = 0;

  //
  // Remap pagetable [2M-4K , 2M+4K] with ReadWrite as 1
  // However, since [2M-4K , 2M+] is present, need 3 pages, return buffer too small, and pagetable unchanged
  //
  // Page table layout is as below:
  //
  // [IA32_CR3*]
  //     |
  //     |
  //     V
  //  [IA32_PML4E*] --> [IA32_PDPTE]
  //                     ...
  //                   [IA32_PDPTE*] --> [IA32_PDE_2M]
  //                                        ...
  //                                    [IA32_PDE]      --> [IA32_PTE_4K]  --> 4K aligned physical address
  //                                                        ...
  //                                                        [IA32_PTE_4K]  --> 4K aligned physical address
  //                                    [IA32_PDE_2M]
  // The one marked with * will be created a new one. so need three pages
  //
  PageTableBufferSize = 0;
  NewPageTable        = PageTable;

  Status = PageTableRemapWritable (&NewPageTable, 4, NULL, &PageTableBufferSize, (UINT64)SIZE_2MB -SIZE_4KB, (UINT64)SIZE_8KB);
  UT_ASSERT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  UT_ASSERT_EQUAL (PageTableBufferSize, EFI_PAGES_TO_SIZE (3));

  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableRemapWritable (&NewPageTable, 4, Buffer, &PageTableBufferSize, (UINT64)SIZE_2MB -SIZE_4KB, (UINT64)SIZE_8KB);
  UT_ASSERT_NOT_EQUAL (NewPageTable, PageTable);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  MapCount = 0;
  Status   = PageTableParse ((UINTN)NewPageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount * sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse ((UINTN)NewPageTable, PagingMode, Map, &MapCount);

  UT_ASSERT_EQUAL (MapCount, 2);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, (UINT64)SIZE_2MB);
  MapAttribute.Bits.ReadWrite = 1;
  UT_ASSERT_EQUAL (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[0].Attribute), IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute));
  MapAttribute.Bits.ReadWrite = 0;

  UT_ASSERT_EQUAL (Map[1].LinearAddress, (UINT64)SIZE_2MB + SIZE_4KB);
  UT_ASSERT_EQUAL (Map[1].Length, (UINT64)SIZE_2MB*4 - SIZE_4KB);

  UT_ASSERT_EQUAL (IA32_MAP_ATTRIBUTE_ATTRIBUTES (&Map[1].Attribute), IA32_MAP_ATTRIBUTE_ATTRIBUTES (&MapAttribute));
  return UNIT_TEST_PASSED;
}

/**
  Check if the parent entry has different Nx attribute

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
TestCaseManualChangeNx (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN               PageTable;
  PAGING_MODE         PagingMode;
  VOID                *Buffer;
  UINTN               PageTableBufferSize;
  IA32_MAP_ATTRIBUTE  MapAttribute;
  IA32_MAP_ATTRIBUTE  ExpectedMapAttribute;
  IA32_MAP_ATTRIBUTE  MapMask;
  RETURN_STATUS       Status;
  IA32_MAP_ENTRY      *Map;
  UINTN               MapCount;
  IA32_PAGING_ENTRY   *PagingEntry;
  UNIT_TEST_STATUS    TestStatus;

  PagingMode                = Paging4Level1GB;
  PageTableBufferSize       = 0;
  PageTable                 = 0;
  Buffer                    = NULL;
  MapAttribute.Uint64       = 0;
  MapMask.Uint64            = MAX_UINT64;
  MapAttribute.Bits.Present = 1;
  MapAttribute.Bits.Nx      = 0;

  //
  // Create Page table to cover [0,2G], with Nx = 0
  //
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_1GB * 2, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES (PageTableBufferSize));
  Status = PageTableMap (&PageTable, PagingMode, Buffer, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_1GB * 2, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  MapCount = 0;
  Status   = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount* sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_2GB);
  ExpectedMapAttribute.Uint64 =   MapAttribute.Uint64;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  //
  // Manually change Nx to 1 for non-leaf entry, which covers [0,2G]
  //
  PagingEntry         = (IA32_PAGING_ENTRY *)(UINTN)PageTable;
  PagingEntry->Uint64 = PagingEntry->Uint64 | BIT63;
  MapCount            = 0;
  Status              = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount* sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  UT_ASSERT_EQUAL (MapCount, 1);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_2GB);
  ExpectedMapAttribute.Bits.Nx = 1;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);

  //
  // Call library to change Nx to 0 for [0,1G]
  //
  Status = PageTableMap (&PageTable, PagingMode, NULL, &PageTableBufferSize, (UINT64)0, (UINT64)SIZE_1GB, &MapAttribute, &MapMask);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);
  TestStatus = IsPageTableValid (PageTable, PagingMode);
  if (TestStatus != UNIT_TEST_PASSED) {
    return TestStatus;
  }

  MapCount = 0;
  Status   = PageTableParse (PageTable, PagingMode, NULL, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  Map    = AllocatePages (EFI_SIZE_TO_PAGES (MapCount* sizeof (IA32_MAP_ENTRY)));
  Status = PageTableParse (PageTable, PagingMode, Map, &MapCount);
  UT_ASSERT_EQUAL (Status, RETURN_SUCCESS);

  //
  // There should be two ranges [0, 1G] with Nx = 0 and [1G, 2G] with Nx = 1
  //
  UT_ASSERT_EQUAL (MapCount, 2);
  UT_ASSERT_EQUAL (Map[0].LinearAddress, 0);
  UT_ASSERT_EQUAL (Map[0].Length, SIZE_1GB);
  ExpectedMapAttribute.Uint64 =   MapAttribute.Uint64;
  UT_ASSERT_EQUAL (Map[0].Attribute.Uint64, ExpectedMapAttribute.Uint64);
  UT_ASSERT_EQUAL (Map[1].LinearAddress, SIZE_1GB);
  UT_ASSERT_EQUAL (Map[1].Length, SIZE_1GB);
  ExpectedMapAttribute.Uint64       =   SIZE_1GB;
  ExpectedMapAttribute.Bits.Present = 1;
  ExpectedMapAttribute.Bits.Nx      = 1;
  UT_ASSERT_EQUAL (Map[1].Attribute.Uint64, ExpectedMapAttribute.Uint64);
  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for the
  sample unit tests and run the unit tests.

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      ManualTestCase;
  UNIT_TEST_SUITE_HANDLE      RandomTestCase;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  //
  // Start setting up the test framework for running the tests.
  //
  Status = InitUnitTestFramework (&Framework, UNIT_TEST_APP_NAME, gEfiCallerBaseName, UNIT_TEST_APP_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
    goto EXIT;
  }

  //
  // Populate the Manual Test Cases.
  //
  Status = CreateUnitTestSuite (&ManualTestCase, Framework, "Manual Test Cases", "CpuPageTableLib.Manual", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Manual Test Cases\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (ManualTestCase, "Check if the input parameters are not supported.", "Manual Test Case1", TestCaseForParameter, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check the case that modifying page table doesn't need extra buffer", "Manual Test Case2", TestCaseWhichNoNeedExtraSize, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check the case to map [0, 1G] to [8K, 1G+8K]", "Manual Test Case3", TestCase1Gmapto4K, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check won't merge entries", "Manual Test Case4", TestCaseManualNotMergeEntry, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check if the parent entry has different ReadWrite attribute", "Manual Test Case5", TestCaseManualChangeReadWrite, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check if the parent entry has different Nx attribute", "Manual Test Case6", TestCaseManualChangeNx, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check if the needed size is expected", "Manual Test Case7", TestCaseManualSizeNotMatch, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check PageTableRemapWritable", "Manual Test Case8", TestCaseForPageTableRemapWritable, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check for minimal size usage of PageTableRemapWritable", "Manual Test Case9", TestCaseForSizeUsageOfPageTableRemapWritable, NULL, NULL, NULL);
  AddTestCase (ManualTestCase, "Check for minimal size usage of PageTableRemapWritable with 4K page entry", "Manual Test Case9", TestCaseForSizeUsageOfPageTableRemapWritable4K, NULL, NULL, NULL);
  //
  // Populate the Random Test Cases.
  //
  Status = CreateUnitTestSuite (&RandomTestCase, Framework, "Random Test Cases", "CpuPageTableLib.Random", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for Random Test Cases\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  AddTestCase (RandomTestCase, "Random Test for Paging4Level", "Random Test Case1", TestCaseforRandomTest, NULL, NULL, &mTestContextPaging4Level);
  AddTestCase (RandomTestCase, "Random Test for Paging4Level1G", "Random Test Case2", TestCaseforRandomTest, NULL, NULL, &mTestContextPaging4Level1GB);
  AddTestCase (RandomTestCase, "Random Test for Paging5Level", "Random Test Case3", TestCaseforRandomTest, NULL, NULL, &mTestContextPaging5Level);
  AddTestCase (RandomTestCase, "Random Test for Paging5Level1G", "Random Test Case4", TestCaseforRandomTest, NULL, NULL, &mTestContextPaging5Level1GB);

  //
  // Execute the tests.
  //
  Status = RunAllTestSuites (Framework);

EXIT:
  if (Framework) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

/**
  Standard POSIX C entry point for host based unit test execution.

  @param Argc  Number of arguments.
  @param Argv  Array of arguments.

  @return Test application exit code.
**/
INT32
main (
  INT32  Argc,
  CHAR8  *Argv[]
  )
{
  InitGlobalData (52);
  return UefiTestMain ();
}
