#ifndef SWC_REGION_H
#define SWC_REGION_H

#include <stdint.h>

struct wl_client;

struct wl_resource *
region_new(struct wl_client *client, uint32_t version, uint32_t id);

#endif
