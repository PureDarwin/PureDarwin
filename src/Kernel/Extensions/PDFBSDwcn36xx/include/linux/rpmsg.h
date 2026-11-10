// rpmsg.h - the rpmsg endpoint calls wcn36xx makes, backed by PDQcomWCNSS's smd channels
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PD_LINUX_RPMSG_H
#define PD_LINUX_RPMSG_H

#include <linux/types.h>

struct rpmsg_device;
struct rpmsg_endpoint;

typedef int (*rpmsg_rx_cb_t)(struct rpmsg_device *, void *, int, void *, u32);

int rpmsg_send(struct rpmsg_endpoint *ept, void *data, int len);
void rpmsg_destroy_ept(struct rpmsg_endpoint *ept);

#endif
