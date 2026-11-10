// net/if_dl.h for Argon on Linux: getifaddrs reports link addresses as AF_PACKET sockaddr_ll
#pragma once
#include <sys/socket.h>
#include <linux/if_packet.h>
#ifndef AF_LINK
#define AF_LINK AF_PACKET
#endif
// sockaddr_dl's fields over sockaddr_ll, the hardware address is where LLADDR points
#define sockaddr_dl sockaddr_ll
#define sdl_alen sll_halen
#define sdl_family sll_family
#define sdl_index sll_ifindex
#define LLADDR(s) ((char *)(s)->sll_addr)
