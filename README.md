# PureDarwin   [![PureDarwin Discord](https://dcbadge.limes.pink/api/server/https://discord.gg/9kz8XXRRcT?style=flat)](https://discord.gg/9kz8XXRRcT)

![logo-sm](https://github.com/user-attachments/assets/ea4bd560-3738-4486-80ab-2f313e4a33a1)

Darwin is the Open Source operating system from Apple that forms the basis for Mac OS X and PureDarwin. PureDarwin is a community project that aims to make Darwin more usable (some people think of it as the informal successor to OpenDarwin).

PureDarwin boots to a shell on four targets: x86_64, arm64 under QEMU, the Raspberry Pi 3, and the iPad 6. On x86_64 it also runs X11 and Wayland desktops.

See the [Website](https://www.puredarwin.org) for more information, and the [Handbook](https://www.puredarwin.org/handbook/_Handbook.html) for developer documentation.

## Building PureDarwin

Nix is the build system. On a Linux host with flakes enabled:

```
nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
nix build .#image-minimal
nix run .#vm
```

The Apple SDK is proprietary and cannot be redistributed, so you have to supply it yourself; the build tells you this if it is missing. Removing that requirement is on the roadmap.

macOS hosts build components, including the kernel and libSystem, but not images, kernel collections or the QEMU runners; finishing that path is on the roadmap. Building the tree by invoking `cmake` directly is not supported.

See [Getting Started](https://www.puredarwin.org/handbook/getting-started.html) for the full version.

## Contributing

Development happens on `next`; releases are frozen onto `main`. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request, and `CODING_STYLE.md` for how code should look.
