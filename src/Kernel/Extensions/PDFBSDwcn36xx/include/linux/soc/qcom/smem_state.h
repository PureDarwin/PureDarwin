// smem_state.h - the shared state bits wcn36xx raises for pronto, the apps smsm word here
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PD_LINUX_SOC_QCOM_SMEM_STATE_H
#define PD_LINUX_SOC_QCOM_SMEM_STATE_H

#include <linux/types.h>

struct device;
struct qcom_smem_state;

struct qcom_smem_state *qcom_smem_state_get(struct device *dev, const char *con_id, unsigned *bit);
void qcom_smem_state_put(struct qcom_smem_state *state);
int qcom_smem_state_update_bits(struct qcom_smem_state *state, u32 mask, u32 value);

#endif
