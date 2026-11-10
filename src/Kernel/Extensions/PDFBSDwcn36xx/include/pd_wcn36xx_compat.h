// pd_wcn36xx_compat.h - what wcn36xx needs that this linuxkpi lacks
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PD_WCN36XX_COMPAT_H
#define PD_WCN36XX_COMPAT_H

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <net/mac80211.h>

// element ids and ht field values from ieee std 802.11-2016, 9.4.2.1, 9.4.2.56 and 9.4.2.57
#ifndef WLAN_EID_EDCA_PARAM_SET
#define WLAN_EID_EDCA_PARAM_SET			12
#endif
#ifndef WLAN_EID_PWR_CONSTRAINT
#define WLAN_EID_PWR_CONSTRAINT			32
#endif
#ifndef WLAN_EID_ERP_INFO
#define WLAN_EID_ERP_INFO			42
#endif
#ifndef WLAN_EID_QOS_CAPA
#define WLAN_EID_QOS_CAPA			46
#endif
#ifndef WLAN_EID_VHT_OPERATION
#define WLAN_EID_VHT_OPERATION			192
#endif
#ifndef WLAN_EID_OPMODE_NOTIF
#define WLAN_EID_OPMODE_NOTIF			199
#endif
#ifndef IEEE80211_HT_CAP_DELAY_BA
#define IEEE80211_HT_CAP_DELAY_BA		0x0400
#endif
#ifndef IEEE80211_HT_PARAM_CHA_SEC_NONE
#define IEEE80211_HT_PARAM_CHA_SEC_NONE		0x00
#define IEEE80211_HT_PARAM_CHA_SEC_ABOVE	0x01
#define IEEE80211_HT_PARAM_CHA_SEC_BELOW	0x03
#endif
#ifndef IEEE80211_HT_OP_MODE_NON_GF_STA_PRSNT
#define IEEE80211_HT_OP_MODE_NON_GF_STA_PRSNT	0x0004
#endif

// the newer name of linuxkpi's rx_enc_flags
#define mac80211_rx_encoding_flags		rx_enc_flags

#ifndef IRQF_TRIGGER_HIGH
#define IRQF_TRIGGER_HIGH			0x00000004
#endif

static inline int
enable_irq_wake(unsigned int irq)
{
	(void)irq;
	return 0;
}

#ifndef dma_rmb
#define dma_rmb()				rmb()
#endif
#ifndef dma_wmb
#define dma_wmb()				wmb()
#endif

static inline void
add_device_randomness(const void *buf, unsigned int len)
{
	(void)buf;
	(void)len;
}

// a struct with a trailing array of count members
#define kmalloc_flex(var, member, count, gfp)	\
	kmalloc(sizeof(var) + sizeof((var).member[0]) * (count), gfp)

// runs func once, the first time cond holds, and yields cond
#define DO_ONCE_LITE_IF(cond, func, ...) ({			\
	static bool __pd_done;					\
	bool __pd_cond = !!(cond);				\
	if (__pd_cond && !__pd_done) {				\
		__pd_done = true;				\
		func(__VA_ARGS__);				\
	}							\
	__pd_cond;						\
})

// ht from the channel width, and the secondary channel from where the centre sits
static inline bool
conf_is_ht(struct ieee80211_conf *conf)
{
	return conf->chandef.width != NL80211_CHAN_WIDTH_5 && conf->chandef.width != NL80211_CHAN_WIDTH_10 &&
	    conf->chandef.width != NL80211_CHAN_WIDTH_20_NOHT;
}

static inline bool
conf_is_ht40_minus(struct ieee80211_conf *conf)
{
	return conf->chandef.width == NL80211_CHAN_WIDTH_40 && conf->chandef.chan != NULL &&
	    conf->chandef.center_freq1 < conf->chandef.chan->center_freq;
}

static inline bool
conf_is_ht40_plus(struct ieee80211_conf *conf)
{
	return conf->chandef.width == NL80211_CHAN_WIDTH_40 && conf->chandef.chan != NULL &&
	    conf->chandef.center_freq1 > conf->chandef.chan->center_freq;
}

// pronto's event work needs no freezing here, there is no suspend
#define create_freezable_workqueue(name)	alloc_ordered_workqueue(name, 0)

// device-tree interrupts through the shim, linuxkpi's request_irq handles pci devices only
int pd_wcn36xx_request_irq(unsigned int irq, irq_handler_t handler, void *arg);
void pd_wcn36xx_free_irq(unsigned int irq, void *arg);

// the device table only feeds module autoloading, which has no use here
#define MODULE_DEVICE_TABLE_BUS_of(_bus, _table)

#endif
