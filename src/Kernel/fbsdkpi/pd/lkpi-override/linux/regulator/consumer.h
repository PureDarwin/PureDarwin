// SPDX-License-Identifier: MIT
// regulator consumers over the providers in pd_providers.h
#ifndef _LINUXKPI_LINUX_REGULATOR_CONSUMER_H_
#define _LINUXKPI_LINUX_REGULATOR_CONSUMER_H_

#include <linux/err.h>
// drm relies on suspend.h coming through here, rtw89 has no vm/swap_pager.h for it
#if __has_include(<vm/swap_pager.h>)
#include <linux/suspend.h>
#endif

struct device;
struct regulator;

struct regulator *regulator_get(struct device *dev, const char *id);
struct regulator *devm_regulator_get(struct device *dev, const char *id);
void regulator_put(struct regulator *r);
int regulator_enable(struct regulator *r);
int regulator_disable(struct regulator *r);
int regulator_get_voltage(struct regulator *r);
int regulator_set_voltage(struct regulator *r, int min_uv, int max_uv);

#endif
