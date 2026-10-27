// SPDX-License-Identifier: MIT
// i2c adapters for drm drivers, bit-banged ones drive their i2c_algo_bit_data lines
// the bit-bang follows the i2c bus specification (NXP UM10204), standard mode timing

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/jiffies.h>

// nothing enumerates i2c buses on PD, a driver keeps its adapter and passes it around
int
lkpi_i2c_add_adapter(struct i2c_adapter *adapter)
{
	return 0;
}

int
lkpi_i2c_del_adapter(struct i2c_adapter *adapter)
{
	return 0;
}

// both lines released is the idle bus
int
lkpi_i2c_bit_add_bus(struct i2c_adapter *adapter)
{
	struct i2c_algo_bit_data *bd = adapter->algo_data;

	if (bd == NULL || bd->setsda == NULL || bd->setscl == NULL || bd->getsda == NULL)
		return -EINVAL;
	if (adapter->retries == 0)
		adapter->retries = 3;
	bd->setsda(bd->data, 1);
	bd->setscl(bd->data, 1);
	return lkpi_i2c_add_adapter(adapter);
}

static void
pd_bb_delay(struct i2c_algo_bit_data *bd)
{
	udelay(bd->udelay > 0 ? bd->udelay : 5);
}

static void
pd_bb_sda(struct i2c_algo_bit_data *bd, int state)
{
	bd->setsda(bd->data, state);
	pd_bb_delay(bd);
}

static void
pd_bb_scl_lo(struct i2c_algo_bit_data *bd)
{
	bd->setscl(bd->data, 0);
	pd_bb_delay(bd);
}

// a target may hold scl low to stretch the clock, wait for it up to the adapter's timeout
static int
pd_bb_scl_hi(struct i2c_algo_bit_data *bd)
{
	unsigned long end;

	bd->setscl(bd->data, 1);
	if (bd->getscl != NULL) {
		end = jiffies + (bd->timeout > 0 ? bd->timeout : msecs_to_jiffies(100));
		while (!bd->getscl(bd->data)) {
			if (time_after(jiffies, end) && !bd->getscl(bd->data))
				return -ETIMEDOUT;
			udelay(1);
		}
	}
	pd_bb_delay(bd);
	return 0;
}

// sda falls while scl is high, from idle or as a repeated start
static int
pd_bb_start(struct i2c_algo_bit_data *bd)
{
	int ret;

	pd_bb_sda(bd, 1);
	ret = pd_bb_scl_hi(bd);
	if (ret != 0)
		return ret;
	pd_bb_sda(bd, 0);
	pd_bb_scl_lo(bd);
	return 0;
}

// sda rises while scl is high
static void
pd_bb_stop(struct i2c_algo_bit_data *bd)
{
	pd_bb_sda(bd, 0);
	pd_bb_scl_hi(bd);
	pd_bb_sda(bd, 1);
}

// eight bits msb first, then the target pulls sda low on the ninth clock to ack
static int
pd_bb_write_byte(struct i2c_algo_bit_data *bd, uint8_t v)
{
	int i, ack, ret;

	for (i = 7; i >= 0; i--) {
		pd_bb_sda(bd, (v >> i) & 1);
		ret = pd_bb_scl_hi(bd);
		if (ret != 0)
			return ret;
		pd_bb_scl_lo(bd);
	}
	pd_bb_sda(bd, 1);
	ret = pd_bb_scl_hi(bd);
	if (ret != 0)
		return ret;
	ack = !bd->getsda(bd->data);
	pd_bb_scl_lo(bd);
	return ack ? 0 : -ENXIO;
}

// the controller acks every byte but the last of a read
static int
pd_bb_read_byte(struct i2c_algo_bit_data *bd, uint8_t *v, bool ack)
{
	int i, ret;
	uint8_t b = 0;

	pd_bb_sda(bd, 1);
	for (i = 0; i < 8; i++) {
		ret = pd_bb_scl_hi(bd);
		if (ret != 0)
			return ret;
		b = (b << 1) | (bd->getsda(bd->data) ? 1 : 0);
		pd_bb_scl_lo(bd);
	}
	pd_bb_sda(bd, ack ? 0 : 1);
	ret = pd_bb_scl_hi(bd);
	if (ret != 0)
		return ret;
	pd_bb_scl_lo(bd);
	pd_bb_sda(bd, 1);
	*v = b;
	return 0;
}

static int
pd_bb_msg(struct i2c_algo_bit_data *bd, struct i2c_msg *m, bool first)
{
	int i, ret;

	if ((m->flags & I2C_M_NOSTART) == 0 || first) {
		ret = pd_bb_start(bd);
		if (ret != 0)
			return ret;
		ret = pd_bb_write_byte(bd, (uint8_t)((m->addr << 1) | ((m->flags & I2C_M_RD) ? 1 : 0)));
		if (ret != 0)
			return ret;
	}
	for (i = 0; i < m->len; i++) {
		if (m->flags & I2C_M_RD)
			ret = pd_bb_read_byte(bd, &m->buf[i], i + 1 < m->len);
		else
			ret = pd_bb_write_byte(bd, m->buf[i]);
		if (ret == -ENXIO)
			return -EIO;
		if (ret != 0)
			return ret;
	}
	return 0;
}

// algo-less adapters with i2c_algo_bit_data land here through i2c_transfer
int
lkpi_i2cbb_transfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int nmsgs)
{
	struct i2c_algo_bit_data *bd = adapter->algo_data;
	int i, ret = 0;

	if (bd == NULL)
		return -EOPNOTSUPP;
	if (bd->pre_xfer != NULL) {
		ret = bd->pre_xfer(adapter);
		if (ret < 0)
			return ret;
	}
	for (i = 0; i < nmsgs; i++) {
		ret = pd_bb_msg(bd, &msgs[i], i == 0);
		if (ret != 0)
			break;
		if ((msgs[i].flags & I2C_M_STOP) && i + 1 < nmsgs)
			pd_bb_stop(bd);
	}
	pd_bb_stop(bd);
	if (bd->post_xfer != NULL)
		bd->post_xfer(adapter);
	return ret != 0 ? ret : nmsgs;
}
