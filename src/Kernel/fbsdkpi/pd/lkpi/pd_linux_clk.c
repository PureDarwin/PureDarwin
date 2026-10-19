// SPDX-License-Identifier: MIT
// clk, reset and power-domain consumers over providers registered by soc services

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/pd_providers.h>

enum { PD_PROV_CLK, PD_PROV_RESET, PD_PROV_POWER };

struct pd_provider {
	int		kind;
	const char	*compatible;
	const void	*ops;
	void		*priv;
};

static struct pd_provider sProviders[32];
static int sProviderCount;
// set up on first use, which is single threaded (module init or the first attach)
static struct mutex pd_provider_lock;
static int pd_provider_lock_ready;

static struct mutex *
pd_provider_lock_get(void)
{
	if (!pd_provider_lock_ready) {
		mutex_init(&pd_provider_lock);
		pd_provider_lock_ready = 1;
	}
	return (&pd_provider_lock);
}

static int
provider_add(int kind, const char *compatible, const void *ops, void *priv)
{
	int error = 0;

	mutex_lock(pd_provider_lock_get());
	if (sProviderCount == nitems(sProviders))
		error = -ENOSPC;
	else
		sProviders[sProviderCount++] = (struct pd_provider){ kind, compatible, ops, priv };
	mutex_unlock(pd_provider_lock_get());
	return (error);
}

int
pd_clk_provider_add(const char *compatible, const struct pd_clk_ops *ops, void *priv)
{
	return (provider_add(PD_PROV_CLK, compatible, ops, priv));
}

int
pd_reset_provider_add(const char *compatible, const struct pd_reset_ops *ops, void *priv)
{
	return (provider_add(PD_PROV_RESET, compatible, ops, priv));
}

int
pd_power_provider_add(const char *compatible, const struct pd_power_ops *ops, void *priv)
{
	return (provider_add(PD_PROV_POWER, compatible, ops, priv));
}

static const struct pd_provider *
provider_for(int kind, const struct device_node *np)
{
	const struct pd_provider *best = NULL;

	mutex_lock(pd_provider_lock_get());
	for (int i = 0; i < sProviderCount; i++) {
		if (sProviders[i].kind == kind && of_device_is_compatible(np, sProviders[i].compatible)) {
			best = &sProviders[i];
			break;
		}
	}
	mutex_unlock(pd_provider_lock_get());
	return (best);
}

// the index-th (or named) entry of a list property such as clocks, resolved to a provider handle
static int
lookup_np(int kind, struct device_node *np, const char *list, const char *cells, const char *names,
    const char *id, int index, const struct pd_provider **prov, void **hw)
{
	struct of_phandle_args args;
	void *(*xlate)(void *, const struct of_phandle_args *);

	if (np == NULL)
		return (-ENODEV);
	if (id != NULL && (index = of_property_match_string(np, names, id)) < 0)
		return (-ENOENT);
	if (of_parse_phandle_with_args(np, list, cells, index, &args) != 0)
		return (-ENOENT);
	*prov = provider_for(kind, args.np);
	if (*prov == NULL) {
		printf("pd platform: no provider for %s %s of %s on %s\n", list, id != NULL ? id : "",
		    np->full_name, args.np->full_name);
		return (-EPROBE_DEFER);
	}
	xlate = kind == PD_PROV_CLK ? ((const struct pd_clk_ops *)(*prov)->ops)->xlate :
	    kind == PD_PROV_RESET ? ((const struct pd_reset_ops *)(*prov)->ops)->xlate :
	    ((const struct pd_power_ops *)(*prov)->ops)->xlate;
	*hw = xlate((*prov)->priv, &args);
	return (*hw != NULL ? 0 : -EINVAL);
}

static int
lookup(int kind, struct device *dev, const char *list, const char *cells, const char *names,
    const char *id, int index, const struct pd_provider **prov, void **hw)
{
	if (dev == NULL)
		return (-ENODEV);
	return (lookup_np(kind, dev->of_node, list, cells, names, id, index, prov, hw));
}

// clocks

struct clk {
	const struct pd_clk_ops	*ops;
	void			*hw;
	int			prepared, enabled;
};

static struct clk *
clk_lookup(struct device *dev, const char *id, int index)
{
	const struct pd_provider *prov;
	struct clk *clk;
	void *hw;
	int error;

	error = lookup(PD_PROV_CLK, dev, "clocks", "#clock-cells", "clock-names", id, index, &prov, &hw);
	if (error != 0)
		return (ERR_PTR(error));
	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	clk->ops = prov->ops;
	clk->hw = hw;
	return (clk);
}

// a clock of a node that has no struct device, e.g. an iommu
struct clk *
of_clk_get(struct device_node *np, int index)
{
	const struct pd_provider *prov;
	struct clk *clk;
	void *hw;
	int error;

	error = lookup_np(PD_PROV_CLK, np, "clocks", "#clock-cells", "clock-names", NULL, index, &prov, &hw);
	if (error != 0)
		return (ERR_PTR(error));
	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	clk->ops = prov->ops;
	clk->hw = hw;
	return (clk);
}

struct clk *
clk_get(struct device *dev, const char *id)
{
	return (clk_lookup(dev, id, 0));
}

void
clk_put(struct clk *clk)
{
	if (!IS_ERR_OR_NULL(clk))
		kfree(clk);
}

static void
devm_clk_release(void *arg)
{
	clk_put(arg);
}

struct clk *
devm_clk_get(struct device *dev, const char *id)
{
	struct clk *clk = clk_get(dev, id);

	if (!IS_ERR(clk))
		lkpi_devm_add_action(dev, devm_clk_release, clk);
	return (clk);
}

struct clk *
devm_clk_get_optional(struct device *dev, const char *id)
{
	struct clk *clk = devm_clk_get(dev, id);

	return (PTR_ERR(clk) == -ENOENT ? NULL : clk);
}

struct clk *
devm_clk_get_enabled(struct device *dev, const char *id)
{
	struct clk *clk = devm_clk_get(dev, id);

	if (!IS_ERR(clk) && clk_prepare_enable(clk) != 0)
		return (ERR_PTR(-EIO));
	return (clk);
}

// prepare and enable are one step for these providers, enable carries the counted state
int
clk_prepare(struct clk *clk)
{
	if (!IS_ERR_OR_NULL(clk))
		clk->prepared++;
	return (0);
}

void
clk_unprepare(struct clk *clk)
{
	if (!IS_ERR_OR_NULL(clk) && clk->prepared > 0)
		clk->prepared--;
}

int
clk_enable(struct clk *clk)
{
	int error = 0;

	if (IS_ERR_OR_NULL(clk))
		return (0);
	if (clk->enabled++ == 0 && clk->ops->enable != NULL)
		error = clk->ops->enable(clk->hw);
	if (error != 0)
		clk->enabled--;
	return (error);
}

void
clk_disable(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk) || clk->enabled == 0)
		return;
	if (--clk->enabled == 0 && clk->ops->disable != NULL)
		clk->ops->disable(clk->hw);
}

int
clk_set_rate(struct clk *clk, unsigned long rate)
{
	if (IS_ERR_OR_NULL(clk))
		return (0);
	return (clk->ops->set_rate != NULL ? clk->ops->set_rate(clk->hw, rate) : -EINVAL);
}

unsigned long
clk_get_rate(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk) || clk->ops->get_rate == NULL)
		return (0);
	return (clk->ops->get_rate(clk->hw));
}

long
clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (IS_ERR_OR_NULL(clk) || clk->ops->round_rate == NULL)
		return (rate);
	return (clk->ops->round_rate(clk->hw, rate));
}

int
clk_bulk_get(struct device *dev, int n, struct clk_bulk_data *clks)
{
	for (int i = 0; i < n; i++) {
		clks[i].clk = clk_get(dev, clks[i].id);
		if (IS_ERR(clks[i].clk)) {
			int error = PTR_ERR(clks[i].clk);
			while (--i >= 0)
				clk_put(clks[i].clk);
			return (error);
		}
	}
	return (0);
}

struct pd_bulk {
	int			n;
	struct clk_bulk_data	*clks;
};

static void
devm_bulk_release(void *arg)
{
	struct pd_bulk *b = arg;

	for (int i = 0; i < b->n; i++)
		clk_put(b->clks[i].clk);
	kfree(b);
}

int
devm_clk_bulk_get(struct device *dev, int n, struct clk_bulk_data *clks)
{
	struct pd_bulk *b;
	int error = clk_bulk_get(dev, n, clks);

	if (error != 0)
		return (error);
	b = kzalloc(sizeof(*b), GFP_KERNEL);
	b->n = n;
	b->clks = clks;
	lkpi_devm_add_action(dev, devm_bulk_release, b);
	return (0);
}

int
devm_clk_bulk_get_optional(struct device *dev, int n, struct clk_bulk_data *clks)
{
	for (int i = 0; i < n; i++) {
		clks[i].clk = devm_clk_get_optional(dev, clks[i].id);
		if (IS_ERR(clks[i].clk))
			return (PTR_ERR(clks[i].clk));
	}
	return (0);
}

// every clock in the node's clocks list, named after clock-names when it has them
int
devm_clk_bulk_get_all(struct device *dev, struct clk_bulk_data **out)
{
	int n = dev->of_node != NULL ? of_count_phandle_with_args(dev->of_node, "clocks", "#clock-cells") : 0;
	struct clk_bulk_data *clks;

	if (n <= 0) {
		*out = NULL;
		return (0);
	}
	clks = devm_kzalloc(dev, n * sizeof(*clks), GFP_KERNEL);
	for (int i = 0; i < n; i++) {
		const char *name = NULL;
		of_property_read_string_index(dev->of_node, "clock-names", i, &name);
		clks[i].id = name;
		clks[i].clk = clk_lookup(dev, NULL, i);
		if (IS_ERR(clks[i].clk))
			return (PTR_ERR(clks[i].clk));
		lkpi_devm_add_action(dev, devm_clk_release, clks[i].clk);
	}
	*out = clks;
	return (n);
}

int
clk_bulk_prepare_enable(int n, const struct clk_bulk_data *clks)
{
	for (int i = 0; i < n; i++) {
		int error = clk_prepare_enable(clks[i].clk);
		if (error != 0) {
			while (--i >= 0)
				clk_disable_unprepare(clks[i].clk);
			return (error);
		}
	}
	return (0);
}

void
clk_bulk_disable_unprepare(int n, const struct clk_bulk_data *clks)
{
	while (--n >= 0)
		clk_disable_unprepare(clks[n].clk);
}

// resets

struct reset_control {
	const struct pd_reset_ops	*ops;
	void				*hw;
};

struct reset_control *
reset_control_get(struct device *dev, const char *id)
{
	const struct pd_provider *prov;
	struct reset_control *rc;
	void *hw;
	int error;

	error = lookup(PD_PROV_RESET, dev, "resets", "#reset-cells", "reset-names", id, 0, &prov, &hw);
	if (error != 0)
		return (ERR_PTR(error));
	rc = kzalloc(sizeof(*rc), GFP_KERNEL);
	rc->ops = prov->ops;
	rc->hw = hw;
	return (rc);
}

void
reset_control_put(struct reset_control *rc)
{
	if (!IS_ERR_OR_NULL(rc))
		kfree(rc);
}

static void
devm_reset_release(void *arg)
{
	reset_control_put(arg);
}

struct reset_control *
devm_reset_control_get(struct device *dev, const char *id)
{
	struct reset_control *rc = reset_control_get(dev, id);

	if (!IS_ERR(rc))
		lkpi_devm_add_action(dev, devm_reset_release, rc);
	return (rc);
}

struct reset_control *
devm_reset_control_get_optional(struct device *dev, const char *id)
{
	struct reset_control *rc = devm_reset_control_get(dev, id);

	return (PTR_ERR(rc) == -ENOENT ? NULL : rc);
}

int
reset_control_assert(struct reset_control *rc)
{
	return (IS_ERR_OR_NULL(rc) || rc->ops->assert == NULL ? 0 : rc->ops->assert(rc->hw));
}

int
reset_control_deassert(struct reset_control *rc)
{
	return (IS_ERR_OR_NULL(rc) || rc->ops->deassert == NULL ? 0 : rc->ops->deassert(rc->hw));
}

int
reset_control_reset(struct reset_control *rc)
{
	int error = reset_control_assert(rc);

	if (error == 0) {
		udelay(10);
		error = reset_control_deassert(rc);
	}
	return (error);
}

// power domains, switched on for the life of the device

int
pd_power_domains_on(struct device_node *np)
{
	struct of_phandle_args args;

	for (int i = 0; of_parse_phandle_with_args(np, "power-domains", "#power-domain-cells", i, &args) == 0; i++) {
		const struct pd_provider *prov = provider_for(PD_PROV_POWER, args.np);
		const struct pd_power_ops *ops;
		void *hw;

		if (prov == NULL) {
			printf("pd platform: no power provider for %s, domain %d left as it is\n",
			    args.np->full_name, i);
			continue;
		}
		ops = prov->ops;
		hw = ops->xlate(prov->priv, &args);
		if (hw != NULL && ops->power_on != NULL && ops->power_on(hw) != 0)
			printf("pd platform: power domain %d of %s did not come on\n", i, np->full_name);
	}
	return (0);
}
