# PureDarwin   [![PureDarwin Discord](https://dcbadge.limes.pink/api/server/https://discord.gg/9kz8XXRRcT?style=flat)](https://discord.gg/9kz8XXRRcT)

![logo-sm](https://github.com/user-attachments/assets/ea4bd560-3738-4486-80ab-2f313e4a33a1)

Darwin is the Open Source operating system from Apple that forms the basis for Mac OS X and PureDarwin. PureDarwin is a community project that aims to make Darwin more usable (some people think of it as the informal successor to OpenDarwin).

One current goal of this project is to provide a useful bootable ISO/VM of some recent version of Darwin.

See the [Website](https://www.puredarwin.org) for more information.

## Building PureDarwin

To build PureDarwin, you will need OpenSSL installed, which is used by xar and ld64.
PureDarwin builds only on macOS. It is currently tested with Xcode 14, but should work
with any other modern Xcode.

You will also need zlib, which is used by the DTrace CTF tools used in building the kernel.
