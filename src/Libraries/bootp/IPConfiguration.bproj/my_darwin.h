/*
 * my_darwin.h
 *
 * Apple's build has a header of this name that makes the per-platform build
 * decisions for IPConfiguration; it is not in the open source drop, the same
 * way libsecurity_keychain's SecBase64P.c is referenced but never shipped.
 * wireless.c is the only file that includes it, and the only decision it needs
 * is whether the Wi-Fi code is built.
 */

#ifndef _S_MY_DARWIN_H
#define _S_MY_DARWIN_H

#ifndef NO_WIRELESS
#define NO_WIRELESS
#endif

#endif /* _S_MY_DARWIN_H */
