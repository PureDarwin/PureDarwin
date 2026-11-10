// pd_wcnss_shim.c - rpmsg, wcnss_ctrl and smem_state for wcn36xx over PDQcomWCNSS
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/rpmsg.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/soc/qcom/wcnss_ctrl.h>

void *pd_wcnss_open_channel_polled(const char *name, void (*recv)(void *, const uint8_t *, uint32_t), void *ctx);
void pd_wcnss_poll(void *ch);
int pd_wcnss_send(void *ch, const void *data, uint32_t len);
void pd_wcnss_close_channel(void *ch);
void pd_wcnss_smsm_update(uint32_t mask, uint32_t value);

struct rpmsg_endpoint {
	void			*ch;
	rpmsg_rx_cb_t		cb;
	void			*priv;
	struct delayed_work	poll;
	bool			stopping;
};

struct qcom_smem_state {
	int			unused;
};

static struct qcom_smem_state apps_state;

static void
endpoint_recv(void *ctx, const uint8_t *data, uint32_t len)
{
	struct rpmsg_endpoint *ept = ctx;

	ept->cb(NULL, (void *)data, len, ept->priv, 0);
}

// the callback runs here, in linuxkpi work, so it may complete and queue work as on linux.
// kthread_run did not survive its first call here, so a delayed work item requeues itself each tick
static void
endpoint_poll(struct work_struct *work)
{
	struct rpmsg_endpoint *ept = container_of(to_delayed_work(work), struct rpmsg_endpoint, poll);

	pd_wcnss_poll(ept->ch);
	if (!READ_ONCE(ept->stopping))
		schedule_delayed_work(&ept->poll, 1);
}

struct rpmsg_endpoint *
qcom_wcnss_open_channel(void *wcnss, const char *name, rpmsg_rx_cb_t cb, void *priv)
{
	struct rpmsg_endpoint *ept = kzalloc(sizeof(*ept), GFP_KERNEL);

	(void)wcnss;
	if (ept == NULL)
		return ERR_PTR(-ENOMEM);
	ept->cb = cb;
	ept->priv = priv;
	ept->ch = pd_wcnss_open_channel_polled(name, endpoint_recv, ept);
	if (ept->ch == NULL) {
		kfree(ept);
		return ERR_PTR(-ENODEV);
	}
	INIT_DELAYED_WORK(&ept->poll, endpoint_poll);
	schedule_delayed_work(&ept->poll, 1);
	return ept;
}

// a full fifo drains as pronto reads it
int
rpmsg_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	for (int tries = 0; tries < 1000; tries++) {
		int err = pd_wcnss_send(ept->ch, data, len);
		if (err == 0)
			return 0;
		if (err != -2)
			return -EIO;
		msleep(1);
	}
	return -ETIMEDOUT;
}

void
rpmsg_destroy_ept(struct rpmsg_endpoint *ept)
{
	if (ept == NULL)
		return;
	WRITE_ONCE(ept->stopping, true);
	cancel_delayed_work_sync(&ept->poll);
	pd_wcnss_close_channel(ept->ch);
	kfree(ept);
}

// the bit is the cell after the named phandle, 10 for tx-enable and 9 for tx-rings-empty on the uz801
struct qcom_smem_state *
qcom_smem_state_get(struct device *dev, const char *con_id, unsigned *bit)
{
	struct of_phandle_args args;
	int index = of_property_match_string(dev->of_node, "qcom,smem-state-names", con_id);

	if (index < 0 || of_parse_phandle_with_args(dev->of_node, "qcom,smem-states", "#qcom,smem-state-cells",
	    index, &args) != 0 || args.args_count < 1)
		return ERR_PTR(-ENOENT);
	of_node_put(args.np);
	if (bit != NULL)
		*bit = args.args[0];
	return &apps_state;
}

void
qcom_smem_state_put(struct qcom_smem_state *state)
{
	(void)state;
}

int
qcom_smem_state_update_bits(struct qcom_smem_state *state, u32 mask, u32 value)
{
	(void)state;
	pd_wcnss_smsm_update(mask, value);
	return 0;
}

// device-tree interrupts, polled from work each tick: linuxkpi's request_irq only sees pci devices
// and the handlers each read the dxe interrupt source first, so polling them is safe
struct pd_irq {
	irq_handler_t	handler;
	void		*arg;
	unsigned int	irq;
};

static struct pd_irq pd_irqs[4];
static struct delayed_work pd_irq_poll;
static bool pd_irq_polling;

static void
pd_irq_tick(struct work_struct *work)
{
	(void)work;
	for (int i = 0; i < ARRAY_SIZE(pd_irqs); i++) {
		struct pd_irq *p = &pd_irqs[i];
		irq_handler_t h = READ_ONCE(p->handler);
		if (h != NULL)
			h(p->irq, p->arg);
	}
	if (READ_ONCE(pd_irq_polling))
		schedule_delayed_work(&pd_irq_poll, 1);
}

int
pd_wcn36xx_request_irq(unsigned int irq, irq_handler_t handler, void *arg)
{
	for (int i = 0; i < ARRAY_SIZE(pd_irqs); i++) {
		struct pd_irq *p = &pd_irqs[i];
		if (p->handler != NULL)
			continue;
		p->arg = arg;
		p->irq = irq;
		WRITE_ONCE(p->handler, handler);
		if (!pd_irq_polling) {
			INIT_DELAYED_WORK(&pd_irq_poll, pd_irq_tick);
			WRITE_ONCE(pd_irq_polling, true);
			schedule_delayed_work(&pd_irq_poll, 1);
		}
		return 0;
	}
	return -ENOMEM;
}

void
pd_wcn36xx_free_irq(unsigned int irq, void *arg)
{
	bool any = false;

	for (int i = 0; i < ARRAY_SIZE(pd_irqs); i++) {
		struct pd_irq *p = &pd_irqs[i];
		if (p->handler != NULL && p->irq == irq && p->arg == arg)
			WRITE_ONCE(p->handler, NULL);
		any |= p->handler != NULL;
	}
	if (!any && pd_irq_polling) {
		WRITE_ONCE(pd_irq_polling, false);
		cancel_delayed_work_sync(&pd_irq_poll);
	}
}
