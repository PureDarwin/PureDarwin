// SPDX-License-Identifier: MIT
// clock consumers, resolved through the node's clocks and clock-names to a pd clk provider
#ifndef _LINUXKPI_LINUX_CLK_H
#define	_LINUXKPI_LINUX_CLK_H

#include <linux/device.h>
#include <linux/err.h>

struct clk;

struct clk_bulk_data {
	const char	*id;
	struct clk	*clk;
};

struct device_node;

struct clk *clk_get(struct device *, const char *);
struct clk *of_clk_get(struct device_node *, int);
struct clk *devm_clk_get(struct device *, const char *);
struct clk *devm_clk_get_optional(struct device *, const char *);
struct clk *devm_clk_get_enabled(struct device *, const char *);
void clk_put(struct clk *);
int clk_prepare(struct clk *);
void clk_unprepare(struct clk *);
int clk_enable(struct clk *);
void clk_disable(struct clk *);
int clk_set_rate(struct clk *, unsigned long);
unsigned long clk_get_rate(struct clk *);
long clk_round_rate(struct clk *, unsigned long);
int clk_bulk_get(struct device *, int, struct clk_bulk_data *);
int devm_clk_bulk_get(struct device *, int, struct clk_bulk_data *);
int devm_clk_bulk_get_optional(struct device *, int, struct clk_bulk_data *);
int devm_clk_bulk_get_all(struct device *, struct clk_bulk_data **);
int clk_bulk_prepare_enable(int, const struct clk_bulk_data *);
void clk_bulk_disable_unprepare(int, const struct clk_bulk_data *);

static inline int
clk_prepare_enable(struct clk *clk)
{
	int error = clk_prepare(clk);

	if (error == 0 && (error = clk_enable(clk)) != 0)
		clk_unprepare(clk);
	return (error);
}

static inline void
clk_disable_unprepare(struct clk *clk)
{
	clk_disable(clk);
	clk_unprepare(clk);
}

#endif
