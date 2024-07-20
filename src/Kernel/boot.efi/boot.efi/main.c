#include <x86_64/efibind.h>
#include <lib.h>

void EfiPanicBoot(char *message, char *file, int line) {
    Print(L"Unrecoverable error during boot process: %s\n", message);
    Print(L"Occurred at: %s:%d\n\n", file, line);
    Print(L"Press any key to shut down.\n");

    EFI_INPUT_KEY ignored;
    ST->ConIn->ReadKeyStroke(ST->ConIn, &ignored);

    ST->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_ABORTED, 0, NULL);

    // Should never get here.
    __builtin_unreachable();
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    return EFI_SUCCESS;
}
