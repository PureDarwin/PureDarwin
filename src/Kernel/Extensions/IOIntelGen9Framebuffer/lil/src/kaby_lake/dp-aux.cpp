#include "src/pd_kext_prims.hpp"
#include <lil/intel.h>
#include <lil/imports.h>

#include "src/base.hpp"
#include "src/kaby_lake/dp-aux.hpp"
#include "src/gmbus.hpp"
#include "src/regs.hpp"


namespace {

typedef struct AuxRequest {
	uint8_t request;
	uint32_t address;
	uint8_t size;
	uint8_t tx[16];
} AuxRequest;

typedef struct AuxResponse {
	uint8_t reply, size;
	uint8_t data[20];
} AuxResponse;

uint32_t dp_pack_aux(uint8_t* data, uint8_t size) {
	uint32_t v = 0;
	if(size > 4)
		size = 4;

	for(uint8_t i = 0; i < size; i++)
		v |= ((uint32_t)data[i]) << ((3 - i) * 8);

	return v;
}

void dp_unpack_aux(uint32_t src, uint8_t* data, uint8_t size) {
	if(size > 4)
		size = 4;

	for(uint8_t i = 0; i < size; i++)
		data[i] = (src >> ((3 - i) * 8));
}

uint32_t dp_get_aux_clock_div(LilGpu *lil_gpu, int i) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	if(gpu->gen >= GEN_SKL) {
		return i ? 0 : 1; // Skylake and up will automatically derive the divider
	} else {
		lil_panic("DP Unknown GPU Gen");
	}
}

uint32_t dp_get_aux_send_ctl(LilGpu *lil_gpu, uint8_t txsize) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	if(gpu->gen >= GEN_SKL) {
		return DDI_AUX_CTL_BUSY | DDI_AUX_CTL_DONE | DDI_AUX_CTL_TIMEOUT | DDI_AUX_CTL_TIMEOUT_VAL_MAX | DDI_AUX_CTL_RX_ERR | DDI_AUX_SET_MSG_SIZE(txsize) | DDI_AUX_CTL_SKL_FW_SYNC_PULSE(32) | DDI_AUX_CTL_SKL_SYNC_PULSE(32);
	} else {
		lil_panic("DP Unknown GPU gen");
	}
}

uint32_t dp_aux_wait(struct LilGpu* gpu, LilConnector *con) {
	int timeout = 10;

	while(timeout > 0) {
		if((REG(DDI_AUX_CTL(con->aux_ch)) & DDI_AUX_CTL_BUSY) == 0)
			return REG(DDI_AUX_CTL(con->aux_ch));

		lil_sleep(1);
		timeout--;
	}

	return REG(DDI_AUX_CTL(con->aux_ch));
}

lil::expected<uint8_t, LilError> dp_aux_xfer(struct LilGpu* gpu, LilConnector *con, uint8_t* tx, uint8_t tx_size, uint8_t* rx, uint8_t rxsize) {
	volatile uint32_t *data = REG_PTR(DDI_AUX_DATA(con->aux_ch));

	// Try to wait for any previous AUX cmds
	for(uint8_t i = 0; i < 3; i++) {
		if((REG(DDI_AUX_CTL(con->aux_ch)) & DDI_AUX_CTL_BUSY) == 0)
			break;
		lil_sleep(1);
	}

	if(REG(DDI_AUX_CTL(con->aux_ch)) & DDI_AUX_CTL_BUSY)
		lil_panic("DP AUX Busy");

	if(tx_size > 20 || rxsize > 20)
		lil_panic("DP AUX xfer too big");

	int clock = 0;
	bool done = false;
	uint32_t status = 0;
	uint32_t aux_clock_divider = 0;
	while((aux_clock_divider = dp_get_aux_clock_div(gpu, clock++))) {
		uint32_t send = dp_get_aux_send_ctl(gpu, tx_size);

		for(uint8_t i = 0; i < 5; i++) {
			for(size_t j = 0; j < tx_size; j += 4)
				data[j / 4] = dp_pack_aux(tx + j, tx_size - j);

			REG(DDI_AUX_CTL(con->aux_ch)) = send;

			status = dp_aux_wait(gpu, con);

			REG(DDI_AUX_CTL(con->aux_ch)) = status | DDI_AUX_CTL_DONE | DDI_AUX_CTL_TIMEOUT | DDI_AUX_CTL_RX_ERR;

			if(status & DDI_AUX_CTL_TIMEOUT) {
				lil_sleep(1);
				continue;
			}

			if(status & DDI_AUX_CTL_RX_ERR) {
				lil_sleep(1);
				continue;
			}

			if(status & DDI_AUX_CTL_DONE) {
				done = true;
				break;
			}
		}
	}

	if(!done)
		return lil::unexpected(LilError::LIL_TIMEOUT);

	if(status & DDI_AUX_CTL_TIMEOUT)
		lil_panic("DP AUX Timeout");

	if(status & DDI_AUX_CTL_RX_ERR)
		lil_panic("DP AUX Receive error");


	uint8_t received_bytes = DDI_AUX_GET_MSG_SIZE(status);
	if(received_bytes == 0 || received_bytes > 20)
		lil_panic("DP AUX Unknown recv bytes");

	if(received_bytes > rxsize)
		received_bytes = rxsize;

	for(size_t i = 0; i < received_bytes; i += 4)
		dp_unpack_aux(data[i / 4], rx + i, received_bytes - i);

	return received_bytes;
}

lil::expected<AuxResponse, LilError> dp_aux_cmd(struct LilGpu* gpu, LilConnector *con, AuxRequest req) {
	AuxResponse res = {};
	uint8_t tx[20] = {0};
	uint8_t rx[20] = {0};

	// CMD Header
	tx[0] = (req.request << 4) | ((req.address >> 16) & 0xF);
	tx[1] = (req.address >> 8) & 0xFF;
	tx[2] = req.address & 0xFF;
	tx[3] = req.size - 1;

	uint8_t op = req.request & ~DDI_AUX_I2C_MOT;
	if(op == DDI_AUX_I2C_WRITE || op == DDI_AUX_NATIVE_WRITE) {
		uint8_t tx_size = req.size ? (req.size + 4) : 3,
				rx_size = 2;

		for(size_t i = 0; i < req.size; i++)
			tx[i + 4] = req.tx[i];

		auto ret = dp_aux_xfer(gpu, con, tx, tx_size, rx, rx_size);
		if(ret && *ret > 0) {
			res.reply = rx[0] >> 4;
		} else if(!ret) {
			return lil::unexpected(ret.error());
		}
	} else if(op == DDI_AUX_I2C_READ || op == DDI_AUX_NATIVE_READ) {
		uint8_t tx_size = req.size ? 4 : 3,
				rx_size = req.size + 1;

		auto ret = dp_aux_xfer(gpu, con, tx, tx_size, rx, rx_size);
		if(ret && *ret > 0) {
			res.reply = rx[0] >> 4;
			res.size = *ret - 1;

			for(uint8_t i = 0; i < res.size; i++)
				res.data[i] = rx[i + 1];
		} else if(!ret) {
			return lil::unexpected(ret.error());
		}
	} else {
		lil_panic("Unknown DP AUX cmd");
	}

	return res;
}

} // namespace

namespace kbl::dp::aux {

// TODO: Check res.reply for any NACKs or DEFERs, instead of assuming i2c success, same for dp_aux_i2c_write
LilError i2c_read(struct LilGpu* gpu, LilConnector *con, uint16_t addr, uint8_t len, uint8_t* buf) {
	AuxRequest req = {};
	AuxResponse res = {};
	req.request = DDI_AUX_I2C_READ | DDI_AUX_I2C_MOT;
	req.address = addr;
	req.size = 0;
	auto ret = dp_aux_cmd(gpu, con, req);

	if(!ret)
		return ret.error();

	for(size_t i = 0; i < len; i++) {
		req.size = 1;

		ret = dp_aux_cmd(gpu, con, req);

		if(!ret)
			return ret.error();

		res = *ret;
		buf[i] = res.data[0];
	}

	req.request = DDI_AUX_I2C_READ;
	req.address = 0;
	req.size = 0;
	ret = dp_aux_cmd(gpu, con, req);

	if(!ret)
		return ret.error();

	return LilError::LIL_SUCCESS;
}

LilError i2c_write(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, uint8_t len, uint8_t* buf) {
	AuxRequest req = {};
	req.request = DDI_AUX_I2C_WRITE | DDI_AUX_I2C_MOT;
	req.address = addr;
	req.size = 0;
	auto ret = dp_aux_cmd(gpu, con, req);

	if(!ret)
		return ret.error();

	for(size_t i = 0; i < len; i++) {
		req.size = 1;
		req.tx[0] = buf[i];

		ret = dp_aux_cmd(gpu, con, req);
		if(!ret)
			return ret.error();
	}

	req.request = DDI_AUX_I2C_READ;
	req.address = 0;
	req.size = 0;
	ret = dp_aux_cmd(gpu, con, req);
	if(!ret)
		return ret.error();

	return LilError::LIL_SUCCESS;
}

uint8_t native_read(struct LilGpu* gpu, LilConnector *con, uint16_t addr) {
	AuxRequest req = {};
	req.request = DDI_AUX_NATIVE_READ;
	req.address = addr;
	req.size = 1;
	AuxResponse res = *dp_aux_cmd(gpu, con, req);

	return res.data[0];
}

void native_readn(struct LilGpu* gpu, LilConnector *con, uint16_t addr, size_t n, void *buf) {
	AuxRequest req = {};
	req.request = DDI_AUX_NATIVE_READ;
	req.address = addr;
	req.size = n;
	AuxResponse res = *dp_aux_cmd(gpu, con, req);

	memcpy(buf, res.data, n);
}

void native_write(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, uint8_t v) {
	AuxRequest req = {};
	req.request = DDI_AUX_NATIVE_WRITE;
	req.address = addr;
	req.size = 1;
	req.tx[0] = v;
	dp_aux_cmd(gpu, con, req);
}

void native_writen(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, size_t n, void *buf) {
	AuxRequest req = {};
	req.request = DDI_AUX_NATIVE_WRITE;
	req.address = addr;
	req.size = n;
	memcpy(req.tx, buf, n);
	dp_aux_cmd(gpu, con, req);
}

#define DDC_SEGMENT 0x30
#define DDC_ADDR 0x50
#define EDID_SIZE 128

void read_edid(struct LilGpu* gpu, LilConnector *con, DisplayData* buf) {
	uint32_t block = 0;

	uint8_t segment = block / 2;
	i2c_write(gpu, con, DDC_SEGMENT, 1, &segment);

	uint8_t start = block * EDID_SIZE;
	i2c_write(gpu, con, DDC_ADDR, 1, &start);

	i2c_read(gpu, con, DDC_ADDR, EDID_SIZE, (uint8_t*)buf);
}

#define DP_DUAL_MODE_ADDR 0x40

bool dual_mode_read(LilGpu *gpu, LilConnector *con, uint8_t offset, void *buffer, size_t size) {
	if(gmbus_read(gpu, con, DP_DUAL_MODE_ADDR, offset, size, (uint8_t*)(buffer)) != LilError::LIL_SUCCESS)
		return false;

	return true;
}

} // namespace kbl::dp::aux
