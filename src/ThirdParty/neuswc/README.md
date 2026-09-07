neuswc
------

neuswc is a fork of [swc](https://github.com/michaelforney/swc/) created by [wayland.fyi](https://wayland.fyi). it provides an easy C library interface to make a wayland compositor. it is much smaller than alternatives like wlroots, and easier to use. it is also portable to linux, freebsd, netbsd, and openbsd. if you want to help with that (or anything else) submit a patch to the [mailing list](https://lists.sr.ht/~shrub900/neuswc)

you can view a list of some compositors made with neuswc at the [wayland.fyi website.](https://wayland.fyi)

the refrence compositor implementation is [tohu](https://git.sr.ht/~shrub900/tohu): it uses many of the new features and is small and simple to understand. if you want to make your own compositor, it can be a useful place to refrence how things work, or a base to customize.


neu features
------------

- z axis ordering
- more cursor functions
- zooming
- experimental subsurface support
- fullscreen
- double window borders
- screenshots
- layer shell support
- window decorations
- framebuffer support

neuswc is in active development, we plan to add many more features, and increase compatibility with new wayland protocols.

build
-----

you will need: 
- A C99-compatible compiler
- meson or muon
- pkg-config
- wayland-scanner, wayland-server, wayland-client
- wayland-server, wayland-client
- libdrm (if  building with DRM support) 
- pixman, xkbcommon
- [neuwld](https://git.sr.ht/~shrub900/neuwld)
- libinput on Linux and wscons on BSD
- xcb, xcb-composite, xcb-ewmh and xcb-icccm if you want Xwayland support.

to build, i use muon and samu. you can probably do something similar with meson and ninja.
warning if you use muon, you will need a recent version built from their git for wayland module support, probably not your distro-packaged version. i do:

```
muon setup build
samu -C build
sudo samu -C build install
```

Darwin
------

Darwin can use an IOFramebuffer for display and IOHIDManager for input. This
backend talks to the operating system through IOKit and CoreFoundation and
does not require a platform-specific device node or privileged launcher.

Configure it explicitly with:

```
meson setup build -Dvideo=fb -Dfb=darwin -Dinput=darwin
```

These options are selected automatically when the host system is Darwin.

credits
-------
an extremely large thank you to [michael forney](https://mforney.org) for creating the original [swc](https://github.com/michaelforney/swc) and [wld](https://github.com/michaelforney/wld), without his amazing work none of this would be possible! once they are more stable, we hope some of the changes from our neu* forks will eventually be upstreamed into the original projects.

repositories
------------

- [srcdump](https://srcdump.net/shrub/neuswc)
- [sourcehut](https://git.sr.ht/~shrub900/neuswc)
