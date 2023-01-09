/** @file
  Produce "dp" shell dynamic command.

  Copyright (c) 2017, Intel Corporation. All rights reserved. <BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>

#include <Guid/Performance.h>
#include <Guid/ExtendedFirmwarePerformance.h>
#include <Guid/FirmwarePerformance.h>

#include <Protocol/HiiPackageList.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/UnicodeCollation.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Library/SortLib.h>
#include <Library/HiiLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/PerformanceLib.h>
#include <Protocol/ShellDynamicCommand.h>
#include <Library/PciLib.h>
#include <Library/IoLib.h>

#include <PiDxe.h>
#include <Protocol/MpService.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/AcpiTable.h>
#include <IndustryStandard/Acpi.h>

EFI_HII_HANDLE  mMyResetHiiHandle;

#define POWER_MGMT_REGISTER_Q35(Offset) \
  PCI_LIB_ADDRESS (0, 0x1f, 0, (Offset))

#define ICH9_PMBASE       0x40
#define ICH9_PMBASE_MASK  (BIT15 | BIT14 | BIT13 | BIT12 | BIT11 |           \
                                     BIT10 | BIT9  | BIT8  | BIT7)

VOID
Loop (
  )
{
  DEBUG ((DEBUG_INFO, "MyResetCommand: into loop\n"));
  while (TRUE) {
  }
}

EFI_STATUS
ModifyFacsTable (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  UINTN                        Handle;
  EFI_ACPI_TABLE_VERSION       Version;
  EFI_ACPI_DESCRIPTION_HEADER  *CurrentTable;
  EFI_ACPI_TABLE_PROTOCOL      *AcpiTableProtocol;
  EFI_ACPI_SDT_PROTOCOL        *AcpiSdtProtocol;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->LocateProtocol (
                  &gEfiAcpiSdtProtocolGuid,
                  NULL,
                  (VOID **)&AcpiSdtProtocol
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Try to find MADT table from installed ACPI table
  //
  Index = 0;
  do {
    Status = AcpiSdtProtocol->GetAcpiTable (Index, (EFI_ACPI_SDT_HEADER **)&CurrentTable, &Version, &Handle);
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    Index++;
  } while (CurrentTable->Signature != EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE);

  DEBUG ((DEBUG_INFO, "MyResetCommand: loop function pointer address%lx\n", Loop));
  DEBUG ((DEBUG_INFO, "MyResetCommand: version %lx\n", ((EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE  *)CurrentTable)->Version));
  DEBUG ((DEBUG_INFO, "MyResetCommand: FACS Flags %lx\n", ((EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE  *)CurrentTable)->Flags));

  if ((((EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE  *)CurrentTable)->Flags & EFI_ACPI_4_0_64BIT_WAKE_SUPPORTED_F) != EFI_ACPI_4_0_64BIT_WAKE_SUPPORTED_F) {
    Print (L"\nFACS table don't support 64 bit waking vector\n");
    return EFI_UNSUPPORTED;
  }

  ((EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE  *)CurrentTable)->XFirmwareWakingVector = (UINTN)Loop;
  ((EFI_ACPI_4_0_FIRMWARE_ACPI_CONTROL_STRUCTURE  *)CurrentTable)->OspmFlags            |= EFI_ACPI_4_0_OSPM_64BIT_WAKE__F;
  return EFI_SUCCESS;
}

/**
  This is the shell command handler function pointer callback type.  This
  function handles the command when it is invoked in the shell.

  @param[in] This                   The instance of the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] SystemTable            The pointer to the system table.
  @param[in] ShellParameters        The parameters associated with the command.
  @param[in] Shell                  The instance of the shell protocol used in the context
                                    of processing this command.

  @return EFI_SUCCESS               the operation was successful
  @return other                     the operation failed.
**/
SHELL_STATUS
EFIAPI
MyResetCommandHandler (
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL  *This,
  IN EFI_SYSTEM_TABLE                    *SystemTable,
  IN EFI_SHELL_PARAMETERS_PROTOCOL       *ShellParameters,
  IN EFI_SHELL_PROTOCOL                  *Shell
  )
{
  EFI_STATUS  Status;

  gEfiShellParametersProtocol = ShellParameters;
  gEfiShellProtocol           = Shell;

  Print (L"\nInto MyResetCommandHandler\n");
  Status = ModifyFacsTable ();
  if (EFI_ERROR (Status)) {
    Print (L"\nFailed to modify FACS table, exit\n");
    return Status;
  }

  UINT32  PmBase;
  UINT32  Pm1_CntBase;
  UINT32  Pm1_Cnt;

  PmBase      = 0x1800;
  Pm1_CntBase = PmBase + 4;

  Print (L"\nPmBase = 0x%x \nWill go to sleep\n", PmBase);
  Pm1_Cnt = IoRead32 (Pm1_CntBase);
  Pm1_Cnt = Pm1_Cnt & (~(UINT32)(BIT10|BIT11|BIT12|BIT13));
  Pm1_Cnt = Pm1_Cnt | ((UINT32)(BIT10|BIT12|BIT13));
  IoWrite32 (Pm1_CntBase, Pm1_Cnt);
  return EFI_SUCCESS;
}

/**
  This is the command help handler function pointer callback type.  This
  function is responsible for displaying help information for the associated
  command.

  @param[in] This                   The instance of the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] Language               The pointer to the language string to use.

  @return string                    Pool allocated help string, must be freed by caller
**/
CHAR16 *
EFIAPI
MyResetCommandGetHelp (
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL  *This,
  IN CONST CHAR8                         *Language
  )
{
  return L"Reset";
}

EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL  mMyResetCommandCommand = {
  L"myreset",
  MyResetCommandHandler,
  MyResetCommandGetHelp
};

/**
  Retrieve HII package list from ImageHandle and publish to HII database.

  @param ImageHandle            The image handle of the process.

  @return HII handle.
**/
EFI_HII_HANDLE
InitializeHiiPackage (
  EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                   Status;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;
  EFI_HII_HANDLE               HiiHandle;

  //
  // Retrieve HII package list from ImageHandle
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = gHiiDatabase->NewPackageList (
                           gHiiDatabase,
                           PackageList,
                           NULL,
                           &HiiHandle
                           );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return HiiHandle;
}

/**
  Entry point of Tftp Dynamic Command.

  Produce the DynamicCommand protocol to handle "tftp" command.

  @param ImageHandle            The image handle of the process.
  @param SystemTable            The EFI System Table pointer.

  @retval EFI_SUCCESS           Tftp command is executed successfully.
  @retval EFI_ABORTED           HII package was failed to initialize.
  @retval others                Other errors when executing tftp command.
**/
EFI_STATUS
EFIAPI
MyResetCommandInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiShellDynamicCommandProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mMyResetCommandCommand
                  );
  ASSERT_EFI_ERROR (Status);
  return Status;
}

/**
  Tftp driver unload handler.

  @param ImageHandle            The image handle of the process.

  @retval EFI_SUCCESS           The image is unloaded.
  @retval Others                Failed to unload the image.
**/
EFI_STATUS
EFIAPI
MyResetCommandUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;

  Status = gBS->UninstallProtocolInterface (
                  ImageHandle,
                  &gEfiShellDynamicCommandProtocolGuid,
                  &mMyResetCommandCommand
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}
