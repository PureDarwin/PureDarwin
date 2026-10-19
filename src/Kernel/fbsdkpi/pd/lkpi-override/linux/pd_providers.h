// SPDX-License-Identifier: MIT
// soc services behind clk, reset and power-domain consumers, keyed by the provider node's compatible
#ifndef _LINUXKPI_LINUX_PD_PROVIDERS_H
#define	_LINUXKPI_LINUX_PD_PROVIDERS_H

#include <linux/of.h>

// xlate turns a consumer's phandle arguments into the provider's own handle
struct pd_clk_ops {
	void		*(*xlate)(void *priv, const struct of_phandle_args *spec);
	int		(*enable)(void *hw);
	void		(*disable)(void *hw);
	int		(*set_rate)(void *hw, unsigned long rate);
	unsigned long	(*get_rate)(void *hw);
	long		(*round_rate)(void *hw, unsigned long rate);
};

struct pd_reset_ops {
	void	*(*xlate)(void *priv, const struct of_phandle_args *spec);
	int	(*assert)(void *hw);
	int	(*deassert)(void *hw);
};

struct pd_power_ops {
	void	*(*xlate)(void *priv, const struct of_phandle_args *spec);
	int	(*power_on)(void *hw);
	int	(*power_off)(void *hw);
};

int pd_clk_provider_add(const char *compatible, const struct pd_clk_ops *ops, void *priv);
int pd_reset_provider_add(const char *compatible, const struct pd_reset_ops *ops, void *priv);
int pd_power_provider_add(const char *compatible, const struct pd_power_ops *ops, void *priv);

// powers every domain in the node's power-domains list, done before a platform probe
int pd_power_domains_on(struct device_node *np);

#endif
