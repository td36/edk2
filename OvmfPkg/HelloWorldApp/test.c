#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Uefi.h>
#include <Library/UefiLib.h>

INTN
EFIAPI
ShellAppMain (
    IN UINTN Argc,
    IN CHAR16 **Argv
)
{
    /* code */
    EFI_STATUS                                    Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL        *gGraphicsOutput;
    EFI_GRAPHICS_OUTPUT_BLT_OPERATION       BltOperation;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION           *Info;
    UINTN                                     SizeOfInfo;
    UINT32                                     MaxNumber;

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL ColorTable[4] = {
        {255, 255, 255, 0},
        {255,   0, 255, 0},
        {255,   0,   0, 0},
        {  0, 255,   0, 0}
    };

    Status = gBS->LocateProtocol (
                &gEfiGraphicsOutputProtocolGuid, 
                NULL, 
                (VOID **) &gGraphicsOutput
                );
    if (EFI_ERROR (Status)) {
        return EFI_PROTOCOL_ERROR;
    }

    BltOperation = EfiBltVideoFill;
    for (int i = 0; i < 4; i++) {
        Status = gGraphicsOutput->Blt(gGraphicsOutput, &(ColorTable[i]), BltOperation, 0, 0, 0, 0, 800, 600, 0);
        if (EFI_ERROR (Status)) {
            return EFI_LOAD_ERROR;
        }
    }

    MaxNumber = gGraphicsOutput->Mode->MaxMode;

    Print(L"First Mode :%d\r\n", gGraphicsOutput->Mode->Mode);
    Print(L"MaxModle :%d\r\n", gGraphicsOutput->Mode->MaxMode);
    Print(L"H :%d\r\n", gGraphicsOutput->Mode->Info->HorizontalResolution);
    Print(L"V :%d\r\n", gGraphicsOutput->Mode->Info->VerticalResolution);
    
    Print(L"------------------------------------\r\n");
    Status = gGraphicsOutput->QueryMode (gGraphicsOutput, 0, &SizeOfInfo, &Info);
    Print(L"%d\r\n", Info->HorizontalResolution);
    Print(L"%d\r\n", Info->VerticalResolution);
    Status = gGraphicsOutput->QueryMode (gGraphicsOutput, 1, &SizeOfInfo, &Info);
    Print(L"%d\r\n", Info->HorizontalResolution);
    Print(L"%d\r\n", Info->VerticalResolution);
    Status = gGraphicsOutput->QueryMode (gGraphicsOutput, 2, &SizeOfInfo, &Info);
    Print(L"%d\r\n", Info->HorizontalResolution);
    Print(L"%d\r\n", Info->VerticalResolution);


    Print(L"Now Mode :%d\r\n", gGraphicsOutput->Mode->Mode);
    return EFI_SUCCESS;
}
