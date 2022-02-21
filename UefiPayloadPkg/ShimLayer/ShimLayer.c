/** @file
  ELF Load Image Support

Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "ShimLayer.h"

STATIC UINT32  mTopOfLowerUsableDram = 0;

/**
  Allocates one or more pages of type EfiBootServicesData.

  Allocates the number of pages of MemoryType and returns a pointer to the
  allocated buffer.  The buffer returned is aligned on a 4KB boundary.
  If Pages is 0, then NULL is returned.
  If there is not enough memory availble to satisfy the request, then NULL
  is returned.

  @param   Pages                 The number of 4 KB pages to allocate.
  @return  A pointer to the allocated buffer or NULL if allocation fails.
**/
VOID *
AllocatePages (
  IN UINTN  Pages
  )
{
  EFI_PEI_HOB_POINTERS        Hob;
  EFI_PHYSICAL_ADDRESS        Offset;
  EFI_HOB_HANDOFF_INFO_TABLE  *HobTable;

  Hob.Raw  = GetHobList ();
  HobTable = Hob.HandoffInformationTable;

  if (Pages == 0) {
    return NULL;
  }

  // Make sure allocation address is page alligned.
  Offset = HobTable->EfiFreeMemoryTop & EFI_PAGE_MASK;
  if (Offset != 0) {
    HobTable->EfiFreeMemoryTop -= Offset;
  }

  //
  // Check available memory for the allocation
  //
  if (HobTable->EfiFreeMemoryTop - ((Pages * EFI_PAGE_SIZE) + sizeof (EFI_HOB_MEMORY_ALLOCATION)) < HobTable->EfiFreeMemoryBottom) {
    return NULL;
  }

  HobTable->EfiFreeMemoryTop -= Pages * EFI_PAGE_SIZE;
  BuildMemoryAllocationHob (HobTable->EfiFreeMemoryTop, Pages * EFI_PAGE_SIZE, EfiBootServicesData);

  return (VOID *)(UINTN)HobTable->EfiFreeMemoryTop;
}

/**
   Callback function to find TOLUD (Top of Lower Usable DRAM)

   Estimate where TOLUD (Top of Lower Usable DRAM) resides. The exact position
   would require platform specific code.

   @param MemoryMapEntry         Memory map entry info got from bootloader.
   @param Params                 Not used for now.

  @retval EFI_SUCCESS            Successfully updated mTopOfLowerUsableDram.
**/
EFI_STATUS
FindToludCallback (
  IN MEMORY_MAP_ENTRY  *MemoryMapEntry,
  IN VOID              *Params
  )
{
  //
  // This code assumes that the memory map on this x86 machine below 4GiB is continous
  // until TOLUD. In addition it assumes that the bootloader provided memory tables have
  // no "holes" and thus the first memory range not covered by e820 marks the end of
  // usable DRAM. In addition it's assumed that every reserved memory region touching
  // usable RAM is also covering DRAM, everything else that is marked reserved thus must be
  // MMIO not detectable by bootloader/OS
  //

  //
  // Skip memory types not RAM or reserved
  //
  if ((MemoryMapEntry->Type == E820_UNUSABLE) || (MemoryMapEntry->Type == E820_DISABLED) ||
      (MemoryMapEntry->Type == E820_PMEM))
  {
    return EFI_SUCCESS;
  }

  //
  // Skip resources above 4GiB
  //
  if ((MemoryMapEntry->Base + MemoryMapEntry->Size) > 0x100000000ULL) {
    return EFI_SUCCESS;
  }

  if ((MemoryMapEntry->Type == E820_RAM) || (MemoryMapEntry->Type == E820_ACPI) ||
      (MemoryMapEntry->Type == E820_NVS))
  {
    //
    // It's usable DRAM. Update TOLUD.
    //
    if (mTopOfLowerUsableDram < (MemoryMapEntry->Base + MemoryMapEntry->Size)) {
      mTopOfLowerUsableDram = (UINT32)(MemoryMapEntry->Base + MemoryMapEntry->Size);
    }
  } else {
    //
    // It might be 'reserved DRAM' or 'MMIO'.
    //
    // If it touches usable DRAM at Base assume it's DRAM as well,
    // as it could be bootloader installed tables, TSEG, GTT, ...
    //
    if (mTopOfLowerUsableDram == MemoryMapEntry->Base) {
      mTopOfLowerUsableDram = (UINT32)(MemoryMapEntry->Base + MemoryMapEntry->Size);
    }
  }

  return EFI_SUCCESS;
}

/**
   Callback function to build resource descriptor HOB

   This function build a HOB based on the memory map entry info.
   Only add EFI_RESOURCE_SYSTEM_MEMORY.

   @param MemoryMapEntry         Memory map entry info got from bootloader.
   @param Params                 Not used for now.

  @retval RETURN_SUCCESS        Successfully build a HOB.
**/
EFI_STATUS
MemInfoCallback (
  IN MEMORY_MAP_ENTRY  *MemoryMapEntry,
  IN VOID              *Params
  )
{
  EFI_PHYSICAL_ADDRESS         Base;
  EFI_RESOURCE_TYPE            Type;
  UINT64                       Size;
  EFI_RESOURCE_ATTRIBUTE_TYPE  Attribue;

  //
  // Skip everything not known to be usable DRAM.
  // It will be added later.
  //
  if ((MemoryMapEntry->Type != E820_RAM) && (MemoryMapEntry->Type != E820_ACPI) &&
      (MemoryMapEntry->Type != E820_NVS))
  {
    return RETURN_SUCCESS;
  }

  Type = EFI_RESOURCE_SYSTEM_MEMORY;
  Base = MemoryMapEntry->Base;
  Size = MemoryMapEntry->Size;

  Attribue = EFI_RESOURCE_ATTRIBUTE_PRESENT |
             EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
             EFI_RESOURCE_ATTRIBUTE_TESTED |
             EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
             EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
             EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
             EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE;

  BuildResourceDescriptorHob (Type, Attribue, (EFI_PHYSICAL_ADDRESS)Base, Size);
  DEBUG ((DEBUG_INFO, "buildhob: base = 0x%lx, size = 0x%lx, type = 0x%x\n", Base, Size, Type));

  if (MemoryMapEntry->Type == E820_ACPI) {
    BuildMemoryAllocationHob (Base, Size, EfiACPIReclaimMemory);
  } else if (MemoryMapEntry->Type == E820_NVS) {
    BuildMemoryAllocationHob (Base, Size, EfiACPIMemoryNVS);
  }

  return RETURN_SUCCESS;
}

/**
   Callback function to build resource descriptor HOB

   This function build a HOB based on the memory map entry info.
   It creates only EFI_RESOURCE_MEMORY_MAPPED_IO and EFI_RESOURCE_MEMORY_RESERVED
   resources.

   @param MemoryMapEntry         Memory map entry info got from bootloader.
   @param Params                 A pointer to ACPI_BOARD_INFO.

  @retval EFI_SUCCESS            Successfully build a HOB.
  @retval EFI_INVALID_PARAMETER  Invalid parameter provided.
**/
EFI_STATUS
MemInfoCallbackMmio (
  IN MEMORY_MAP_ENTRY  *MemoryMapEntry,
  IN VOID              *Params
  )
{
  EFI_PHYSICAL_ADDRESS         Base;
  EFI_RESOURCE_TYPE            Type;
  UINT64                       Size;
  EFI_RESOURCE_ATTRIBUTE_TYPE  Attribue;

  //
  // Skip types already handled in MemInfoCallback
  //
  if ((MemoryMapEntry->Type == E820_RAM) || (MemoryMapEntry->Type == E820_ACPI)) {
    return EFI_SUCCESS;
  }

  if (MemoryMapEntry->Base < mTopOfLowerUsableDram) {
    //
    // It's in DRAM and thus must be reserved
    //
    Type = EFI_RESOURCE_MEMORY_RESERVED;
  } else if ((MemoryMapEntry->Base < 0x100000000ULL) && (MemoryMapEntry->Base >= mTopOfLowerUsableDram)) {
    //
    // It's not in DRAM, must be MMIO
    //
    Type = EFI_RESOURCE_MEMORY_MAPPED_IO;
  } else {
    Type = EFI_RESOURCE_MEMORY_RESERVED;
  }

  Base = MemoryMapEntry->Base;
  Size = MemoryMapEntry->Size;

  Attribue = EFI_RESOURCE_ATTRIBUTE_PRESENT |
             EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
             EFI_RESOURCE_ATTRIBUTE_TESTED |
             EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
             EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
             EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
             EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE;

  BuildResourceDescriptorHob (Type, Attribue, (EFI_PHYSICAL_ADDRESS)Base, Size);
  DEBUG ((DEBUG_INFO, "buildhob: base = 0x%lx, size = 0x%lx, type = 0x%x\n", Base, Size, Type));

  if ((MemoryMapEntry->Type == E820_UNUSABLE) ||
      (MemoryMapEntry->Type == E820_DISABLED))
  {
    BuildMemoryAllocationHob (Base, Size, EfiUnusableMemory);
  } else if (MemoryMapEntry->Type == E820_PMEM) {
    BuildMemoryAllocationHob (Base, Size, EfiPersistentMemory);
  }

  return EFI_SUCCESS;
}

/**
  It will build HOBs based on information from bootloaders.

  @retval EFI_SUCCESS        If it completed successfully.
  @retval Others             If it failed to build required HOBs.
**/
EFI_STATUS
BuildHobFromBl (
  VOID
  )
{
  EFI_STATUS                        Status;
  EFI_PEI_GRAPHICS_INFO_HOB         GfxInfo;
  EFI_PEI_GRAPHICS_INFO_HOB         *NewGfxInfo;
  EFI_PEI_GRAPHICS_DEVICE_INFO_HOB  GfxDeviceInfo;
  EFI_PEI_GRAPHICS_DEVICE_INFO_HOB  *NewGfxDeviceInfo;
  UNIVERSAL_PAYLOAD_SMBIOS_TABLE    *SmBiosTableHob;
  UNIVERSAL_PAYLOAD_ACPI_TABLE      *AcpiTableHob;

  //
  // First find TOLUD
  //
  DEBUG ((DEBUG_INFO, "Guessing Top of Lower Usable DRAM:\n"));
  Status = ParseMemoryInfo (FindToludCallback, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_INFO, "Assuming TOLUD = 0x%x\n", mTopOfLowerUsableDram));

  //
  // Parse memory info and build memory HOBs for Usable RAM
  //
  DEBUG ((DEBUG_INFO, "Building ResourceDescriptorHobs for usable memory:\n"));
  Status = ParseMemoryInfo (MemInfoCallback, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Create guid hob for frame buffer information
  //
  Status = ParseGfxInfo (&GfxInfo);
  if (!EFI_ERROR (Status)) {
    NewGfxInfo = BuildGuidHob (&gEfiGraphicsInfoHobGuid, sizeof (GfxInfo));
    ASSERT (NewGfxInfo != NULL);
    CopyMem (NewGfxInfo, &GfxInfo, sizeof (GfxInfo));
    DEBUG ((DEBUG_INFO, "Created graphics info hob\n"));
  }

  Status = ParseGfxDeviceInfo (&GfxDeviceInfo);
  if (!EFI_ERROR (Status)) {
    NewGfxDeviceInfo = BuildGuidHob (&gEfiGraphicsDeviceInfoHobGuid, sizeof (GfxDeviceInfo));
    ASSERT (NewGfxDeviceInfo != NULL);
    CopyMem (NewGfxDeviceInfo, &GfxDeviceInfo, sizeof (GfxDeviceInfo));
    DEBUG ((DEBUG_INFO, "Created graphics device info hob\n"));
  }

  //
  // Creat SmBios table Hob
  //
  SmBiosTableHob = BuildGuidHob (&gUniversalPayloadSmbiosTableGuid, sizeof (UNIVERSAL_PAYLOAD_SMBIOS_TABLE));
  ASSERT (SmBiosTableHob != NULL);
  SmBiosTableHob->Header.Revision = UNIVERSAL_PAYLOAD_SMBIOS_TABLE_REVISION;
  SmBiosTableHob->Header.Length   = sizeof (UNIVERSAL_PAYLOAD_SMBIOS_TABLE);
  DEBUG ((DEBUG_INFO, "Create smbios table gUniversalPayloadSmbiosTableGuid guid hob\n"));
  Status = ParseSmbiosTable (SmBiosTableHob);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Detected Smbios Table at 0x%lx\n", SmBiosTableHob->SmBiosEntryPoint));
  }

  //
  // Creat ACPI table Hob
  //
  AcpiTableHob = BuildGuidHob (&gUniversalPayloadAcpiTableGuid, sizeof (UNIVERSAL_PAYLOAD_ACPI_TABLE));
  ASSERT (AcpiTableHob != NULL);
  AcpiTableHob->Header.Revision = UNIVERSAL_PAYLOAD_ACPI_TABLE_REVISION;
  AcpiTableHob->Header.Length   = sizeof (UNIVERSAL_PAYLOAD_ACPI_TABLE);
  DEBUG ((DEBUG_INFO, "Create ACPI table gUniversalPayloadAcpiTableGuid guid hob\n"));
  Status = ParseAcpiTableInfo (AcpiTableHob);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Detected ACPI Table at 0x%lx\n", AcpiTableHob->Rsdp));
  }
  //
  // Parse memory info and build memory HOBs for reserved DRAM and MMIO
  //
  DEBUG ((DEBUG_INFO, "Building ResourceDescriptorHobs for reserved memory:\n"));
  Status = ParseMemoryInfo (MemInfoCallbackMmio, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  This function will build some generic HOBs that doesn't depend on information from bootloaders.

**/
VOID
BuildGenericHob (
  VOID
  )
{
  EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttribute;

  //Memory allocaion hob for the Shim Layer
  BuildMemoryAllocationHob (0x800000, 0x100000, EfiBootServicesData);

  //
  // Build CPU memory space and IO space hob
  //
  BuildCpuHob (36, 16);

  //
  // Report Local APIC range, cause sbl HOB to be NULL, comment now
  //
  ResourceAttribute = (
                       EFI_RESOURCE_ATTRIBUTE_PRESENT |
                       EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
                       EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
                       EFI_RESOURCE_ATTRIBUTE_TESTED
                       );
  BuildResourceDescriptorHob (EFI_RESOURCE_MEMORY_MAPPED_IO, ResourceAttribute, 0xFEC80000, SIZE_512KB);
  BuildMemoryAllocationHob (0xFEC80000, SIZE_512KB, EfiMemoryMappedIO);
}

EFI_STATUS
ConvertCbmemToHob (
  VOID
  )
{
  UINTN                               MemBase;
  UINTN                               HobMemBase;
  UINTN                               HobMemTop;
  EFI_STATUS                          Status;
  SERIAL_PORT_INFO                    SerialPortInfo;
  UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO  *UniversalSerialPort;

  MemBase    = 0x00800000;
  //Assume the size of shimlayer.elf is 0x100000.
  HobMemBase = ALIGN_VALUE (MemBase + 0x100000, SIZE_1MB);
  HobMemTop  = HobMemBase + 0x800000;
  HobConstructor ((VOID *)MemBase, (VOID *)HobMemTop, (VOID *)HobMemBase, (VOID *)HobMemTop);

  Status = ParseSerialInfo (&SerialPortInfo);
  if (!EFI_ERROR (Status)) {
    UniversalSerialPort = BuildGuidHob (&gUniversalPayloadSerialPortInfoGuid, sizeof (UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO));
    ASSERT (UniversalSerialPort != NULL);
    UniversalSerialPort->Header.Revision = UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO_REVISION;
    UniversalSerialPort->Header.Length   = sizeof (UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO);
    UniversalSerialPort->UseMmio         = (SerialPortInfo.Type == 1) ? FALSE : TRUE;
    UniversalSerialPort->RegisterBase    = SerialPortInfo.BaseAddr;
    UniversalSerialPort->BaudRate        = SerialPortInfo.Baud;
    UniversalSerialPort->RegisterStride  = (UINT8)SerialPortInfo.RegWidth;
  }

  ProcessLibraryConstructorList ();
  DEBUG ((DEBUG_INFO, "sizeof(UINTN) = 0x%x\n", sizeof (UINTN)));
  Status = BuildHobFromBl ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BuildHobFromBl Status = %r\n", Status));
    return Status;
  }

  BuildGenericHob ();
  return EFI_SUCCESS;
}

EFI_STATUS
LoadPayload (
  OUT    EFI_PHYSICAL_ADDRESS    *ImageAddressArg   OPTIONAL,
  OUT    UINT64                  *ImageSizeArg,
  OUT    PHYSICAL_ADDRESS        *UniversalPayloadEntry
  )
{
  EFI_STATUS                     Status;
  UINT32                         Index;
  UINT16                         ExtraDataIndex;
  CHAR8                          *SectionName;
  UINTN                          Offset;
  UINTN                          Size;
  UINTN                          Length;
  UINT32                         ExtraDataCount;
  VOID                           *Elf;
  ELF_IMAGE_CONTEXT              Context;
  UNIVERSAL_PAYLOAD_EXTRA_DATA   *ExtraData;
  UNIVERSAL_PAYLOAD_INFO_HEADER  *PldInfo;

  /* Hard code for now */
  Elf = (VOID *) 0x1600000;
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ParseElfImage (Elf, &Context);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((
    DEBUG_INFO,
    "Payload File Size: 0x%08X, Mem Size: 0x%08x, Reload: %d\n",
    Context.FileSize,
    Context.ImageSize,
    Context.ReloadRequired
    ));

  //
  // Get UNIVERSAL_PAYLOAD_INFO_HEADER and number of additional PLD sections.
  //
  PldInfo        = NULL;
  ExtraDataCount = 0;
  for (Index = 0; Index < Context.ShNum; Index++) {
    Status = GetElfSectionName (&Context, Index, &SectionName);
    if (EFI_ERROR (Status)) {
      continue;
    }

    DEBUG ((DEBUG_INFO, "Payload Section[%d]: %a\n", Index, SectionName));
    if (AsciiStrCmp (SectionName, UNIVERSAL_PAYLOAD_INFO_SEC_NAME) == 0) {
      Status = GetElfSectionPos (&Context, Index, &Offset, &Size);
      if (!EFI_ERROR (Status)) {
        PldInfo = (UNIVERSAL_PAYLOAD_INFO_HEADER *)(Context.FileBase + Offset);
      }
    } else if (AsciiStrnCmp (SectionName, UNIVERSAL_PAYLOAD_EXTRA_SEC_NAME_PREFIX, UNIVERSAL_PAYLOAD_EXTRA_SEC_NAME_PREFIX_LENGTH) == 0) {
      Status = GetElfSectionPos (&Context, Index, &Offset, &Size);
      if (!EFI_ERROR (Status)) {
        ExtraDataCount++;
      }
    }
  }

  //
  // Report the additional PLD sections through HOB.
  //
  Length    = sizeof (UNIVERSAL_PAYLOAD_EXTRA_DATA) + ExtraDataCount * sizeof (UNIVERSAL_PAYLOAD_EXTRA_DATA_ENTRY);
  ExtraData = BuildGuidHob (
                &gUniversalPayloadExtraDataGuid,
                Length
                );
  ExtraData->Count           = ExtraDataCount;
  ExtraData->Header.Revision = UNIVERSAL_PAYLOAD_EXTRA_DATA_REVISION;
  ExtraData->Header.Length   = (UINT16)Length;
  if (ExtraDataCount != 0) {
    for (ExtraDataIndex = 0, Index = 0; Index < Context.ShNum; Index++) {
      Status = GetElfSectionName (&Context, Index, &SectionName);
      if (EFI_ERROR (Status)) {
        continue;
      }

      if (AsciiStrnCmp (SectionName, UNIVERSAL_PAYLOAD_EXTRA_SEC_NAME_PREFIX, UNIVERSAL_PAYLOAD_EXTRA_SEC_NAME_PREFIX_LENGTH) == 0) {
        Status = GetElfSectionPos (&Context, Index, &Offset, &Size);
        if (!EFI_ERROR (Status)) {
          ASSERT (ExtraDataIndex < ExtraDataCount);
          AsciiStrCpyS (
            ExtraData->Entry[ExtraDataIndex].Identifier,
            sizeof (ExtraData->Entry[ExtraDataIndex].Identifier),
            SectionName + UNIVERSAL_PAYLOAD_EXTRA_SEC_NAME_PREFIX_LENGTH
            );
          ExtraData->Entry[ExtraDataIndex].Base = (UINTN)(Context.FileBase + Offset);
          ExtraData->Entry[ExtraDataIndex].Size = Size;
          ExtraDataIndex++;
        }
      }
    }
  }
  if (Context.ReloadRequired || (Context.PreferredImageAddress != Context.FileBase)) {
    Context.ImageAddress = AllocatePages (EFI_SIZE_TO_PAGES (Context.ImageSize));
  } else {
    Context.ImageAddress = Context.FileBase;
  }
  //
  // Load ELF into the required base
  //
  Status = LoadElfImage (&Context);
  if (!EFI_ERROR (Status)) {
    *ImageAddressArg        = (UINTN)Context.ImageAddress;
    *UniversalPayloadEntry  = Context.EntryPoint;
    *ImageSizeArg           = Context.ImageSize;
  }
  return Status;
}

EFI_STATUS
HandOffToPayload (
  IN     PHYSICAL_ADDRESS        UniversalPayloadEntry,
  IN     EFI_PEI_HOB_POINTERS    Hob
  )
{

  EFI_STATUS  Status;
  UINTN       HobList;

  HobList = (UINTN)(VOID *)Hob.Raw;
  typedef EFI_STATUS (EFIAPI *PayloadEntry) (UINTN);
  Status = ((PayloadEntry) UniversalPayloadEntry) (HobList);

  CpuDeadLoop ();
  return EFI_SUCCESS;
}

/**

  Entry point to the C language phase of Shim Layer before UEFI payload.

  @param[in]   BootloaderParameter    The starting address of bootloader parameter block.

  @retval      It will not return if SUCCESS, and return error when passing bootloader parameter.

**/
EFI_STATUS
EFIAPI
_ModuleEntryPoint (
  IN UINTN                      BootloaderParameter
  )
{
  EFI_STATUS              Status;
  EFI_PEI_HOB_POINTERS    Hob;
  PHYSICAL_ADDRESS        ImageAddress;
  UINT64                  ImageSize;
  PHYSICAL_ADDRESS        UniversalPayloadEntry;

  Status = PcdSet64S (PcdBootloaderParameter, BootloaderParameter);
  ASSERT_EFI_ERROR (Status);

  Status = ConvertCbmemToHob();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ConvertCbmemToHob Status = %r\n", Status));
    return Status;
  }

  Status = LoadPayload (&ImageAddress, &ImageSize, &UniversalPayloadEntry);
  ASSERT_EFI_ERROR (Status);
  BuildMemoryAllocationHob (ImageAddress, ImageSize, EfiBootServicesData);

  Hob.HandoffInformationTable = (EFI_HOB_HANDOFF_INFO_TABLE *)GetFirstHob (EFI_HOB_TYPE_HANDOFF);
  HandOffToPayload (UniversalPayloadEntry, Hob);

  // Should not get here
  CpuDeadLoop ();
  return EFI_SUCCESS;
}
