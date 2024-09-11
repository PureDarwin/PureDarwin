#include <x86_64/efibind.h>
#include <lib.h>

void EfiPanicBoot(char *message, char *file, int line) {
    Print(L"Unrecoverable error during boot process: %s\n", message);
    Print(L"Occurred at: %s:%d\n\n", file, line);
    Print(L"Press any key to shut down.\n");

    EFI_INPUT_KEY ignored;
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &ignored);

    uefi_call_wrapper(ST->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_ABORTED, 0, NULL);

    // Should never get here.
    __builtin_unreachable();
}

EFI_STATUS EFI_FUNCTION EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    return EFI_SUCCESS;
}
