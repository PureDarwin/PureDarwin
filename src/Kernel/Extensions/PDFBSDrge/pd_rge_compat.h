// SPDX-License-Identifier: MIT
// freebsd kpi calls rge uses that the haiku compat layer lacks, force-included into the driver
#ifndef PD_RGE_COMPAT_H
#define PD_RGE_COMPAT_H

#define device_log(dev, pri, ...) device_printf((dev), __VA_ARGS__)

// true when the device has a pci power management capability
#define pci_has_pm(dev) ({ int _pm_reg; pci_find_cap((dev), PCIY_PMG, &_pm_reg) == 0; })

void pd_rge_gen_addr(const char *name, unsigned char *eaddr);
#define ether_gen_addr_byname(name, ea) pd_rge_gen_addr((name), (unsigned char *)(ea))

#endif
