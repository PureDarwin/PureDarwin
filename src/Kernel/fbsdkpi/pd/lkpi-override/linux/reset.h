// SPDX-License-Identifier: MIT
// reset consumers, resolved through the node's resets and reset-names to a pd reset provider
#ifndef _LINUXKPI_LINUX_RESET_H
#define	_LINUXKPI_LINUX_RESET_H

#include <linux/device.h>
#include <linux/err.h>

struct reset_control;

struct reset_control *reset_control_get(struct device *, const char *);
struct reset_control *devm_reset_control_get(struct device *, const char *);
struct reset_control *devm_reset_control_get_optional(struct device *, const char *);
void reset_control_put(struct reset_control *);
int reset_control_assert(struct reset_control *);
int reset_control_deassert(struct reset_control *);
int reset_control_reset(struct reset_control *);

#define	reset_control_get_exclusive(d, n)		reset_control_get((d), (n))
#define	reset_control_get_shared(d, n)			reset_control_get((d), (n))
#define	devm_reset_control_get_exclusive(d, n)		devm_reset_control_get((d), (n))
#define	devm_reset_control_get_shared(d, n)		devm_reset_control_get((d), (n))
#define	devm_reset_control_get_optional_exclusive(d, n)	devm_reset_control_get_optional((d), (n))
#define	devm_reset_control_get_optional_shared(d, n)	devm_reset_control_get_optional((d), (n))

#endif
