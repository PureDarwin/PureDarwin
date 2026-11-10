// wcnss_ctrl.h - opening a named channel on the wcnss smd edge
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PD_LINUX_SOC_QCOM_WCNSS_CTRL_H
#define PD_LINUX_SOC_QCOM_WCNSS_CTRL_H

#include <linux/rpmsg.h>

struct rpmsg_endpoint *qcom_wcnss_open_channel(void *wcnss, const char *name, rpmsg_rx_cb_t cb, void *priv);

#endif
