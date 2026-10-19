// SPDX-License-Identifier: MIT
// qwx(4) as PDQwx.cpp sees it, plain C so the c++ side never includes the openbsd headers
#ifndef PD_QWX_GLUE_H
#define PD_QWX_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

struct device;
struct ieee80211com;
struct pci_attach_args;

struct device *pd_qwx_alloc(const char *);
void	pd_qwx_free(struct device *);
int	pd_qwx_match(struct pci_attach_args *);
int	pd_qwx_attach(struct device *, struct pci_attach_args *);
int	pd_qwx_attach_hook(struct device *);
void	pd_qwx_detach(struct device *);
void	pd_qwx_set_variant(struct device *, const char *);
struct ieee80211com *pd_qwx_ic(struct device *);
int	pd_qwx_fwcheck(int);

#ifdef __cplusplus
}
#endif

#endif
