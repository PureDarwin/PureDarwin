#include <x86_64/efibind.h>
#include <lib.h>

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    return EFI_SUCCESS;
}
