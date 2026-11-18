# PureDarwin   [![PureDarwin Discord](https://dcbadge.limes.pink/api/server/https://discord.gg/9kz8XXRRcT?style=flat)](https://discord.gg/9kz8XXRRcT)

![logo-sm](https://github.com/user-attachments/assets/ea4bd560-3738-4486-80ab-2f313e4a33a1)

Darwin is the Open Source operating system from Apple that forms the basis for Mac OS X and PureDarwin. PureDarwin is a community project that aims to make Darwin more usable (some people think of it as the informal successor to OpenDarwin).

PureDarwin builds Darwin from Apple's open source releases and fills the gaps with its own code: drivers, frameworks, a desktop and a Nix build that produces bootable images.

## What PureDarwin does today

- **Targets.** x86_64 PCs and laptops, arm64 under QEMU, the Raspberry Pi 3 and the iPad 6. Apple M1 (t8103) bring-up is in progress.
- **Desktop.** A Wayland desktop (Argon, on our neuswc and neuwld compositor work) with its own applications, and X11.
- **Cocoa frameworks.** PureDarwin's own AppKit, Foundation, CoreFoundation, CoreGraphics, CoreText, QuartzCore and OpenGL, enough to run unmodified macOS applications. The stock macOS build of Firefox has run unmodified since October 2026.
- **Graphics.** AMD, Intel and nouveau GPUs through our in-kernel FreeBSD/Linux compatibility layer (fbsdkpi), Vulkan through Mesa, and NVIDIA Maxwell, Pascal and Volta cards on NVIDIA's own 580 driver through PDNVLoader.
- **Linux binaries.** elftrans runs unmodified Linux programs on XNU, including NVIDIA's userland (nvidia-smi, Vulkan).
- **Drivers and filesystems.** xHCI USB 3 and AHCI storage, HD Audio, Wi-Fi, and a read/write ext4 driver alongside HFS+ and APFS.

## Credit and forks

PureDarwin's own work, the projects it builds on, and how forks should credit it are listed in [NOTICE](NOTICE). Forks are welcome. If you publish one, please say plainly what comes from PureDarwin and what is new in yours, in the places people actually read: READMEs, release notes and announcements.

See the [Website](https://www.puredarwin.org) for more information, and the [Handbook](https://www.puredarwin.org/handbook/_Handbook.html) for developer documentation.

## Building PureDarwin

Nix is the build system. On a Linux host with flakes enabled:

```
nix build .#image-minimal
nix run .#vm
```

The build uses PureDarwin's own open source SDK, so no Apple SDK is needed. Built images and release files are also on the binary cache at https://hydra.lab004.dev.

macOS hosts build components, including the kernel and libSystem, but not images, kernel collections or the QEMU runners. Finishing that path is on the roadmap. Building the tree by invoking `cmake` directly is not supported.

See [Getting Started](https://www.puredarwin.org/handbook/getting-started.html) for the full version.

## Contributing

Development happens on `next`, and releases are frozen onto `main`. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request, and `CODING_STYLE.md` for how code should look.
