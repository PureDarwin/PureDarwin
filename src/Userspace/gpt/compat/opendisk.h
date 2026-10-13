// NetBSD's opendisk: open a disk by path or by bare name under /dev
#ifndef PD_GPT_OPENDISK_H
#define PD_GPT_OPENDISK_H
#include <stddef.h>
int opendisk(const char *path, int flags, char *buf, size_t buflen, int iscooked);
#endif
