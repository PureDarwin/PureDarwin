#include "src/edid.hpp"
#include "src/gmbus.hpp"
#include "src/regs.hpp"

#include <lil/imports.h>
#include <lil/intel.h>

#include <stddef.h>

#define GMBUS_SELECT 0x5100
#define GMBUS_COMMAND_STATUS 0x5104
#define GMBUS_STATUS 0x5108
#define GMBUS_DATA 0x510C
#define GMBUS_IRMASK 0x5110
#define GMBUS_2BYTEINDEX 0x5120

#define GMBUS_HW_RDY (1 << 11)
#define GMBUS_NAK (1 << 10)
#define GMBUS_HW_WAIT_PHASE (1 << 14)

#define GMBUS_CYCLE_WAIT (1 << 25)
#define GMBUS_CYCLE_INDEX (1 << 26)

#define GMBUS_LEN_MASK 511
#define GMBUS_LEN_SHIFT 16
#define GMBUS_LEN(x) ((x & GMBUS_LEN_MASK) << GMBUS_LEN_SHIFT)

#define GMBUS_OFFSET_MASK 127
#define GMBUS_OFFSET_SHIFT 1
#define GMBUS_OFFSET(x) ((x & GMBUS_OFFSET_MASK) << GMBUS_OFFSET_SHIFT)

#define GMBUS_INDEX_MASK 0xFF
#define GMBUS_INDEX_SHIFT 8
#define GMBUS_INDEX(x) ((x & GMBUS_INDEX_MASK) << GMBUS_INDEX_SHIFT)

#define GMBUS_READ 1
#define GMBUS_WRITE 0

#define GMBUS_SW_READY (1 << 30)
#define GMBUS_CLEAR_INTERRUPT (1 << 31)

enum class GmbusStatus {
	Success,
	Nak,
	Timeout,
};

static bool gmbus2_wait_for_mask(LilGpu *gpu, LilConnector *con, uint32_t mask, bool wait_for_unset) {
	size_t tries = 0;
	while(true) {
		uint32_t status = REG(GMBUS2);

		if(wait_for_unset && (status & mask) == 0)
			return true;
		else if(!wait_for_unset && (status & mask) == mask)
			return true;

		if(tries++ > 150000)
			return false;
	}

	return true;
}

static uint8_t ddc_pin(LilConnector *con) {
	switch(con->type) {
		case DISPLAYPORT:
			return con->encoder->dp.ddc_pin;
		case HDMI:
			return con->encoder->hdmi.ddc_pin;
		case LVDS:
			return 0b11;
		default:
			lil_panic("DDC pin unimplemented for this connector type");
	}
}

static void gmbus_setup(LilGpu* gpu, LilConnector *con) {
	REG(GMBUS0) = ddc_pin(con);

	uint32_t status = REG(GMBUS2);

	if(status & (GMBUS_NAK)) {
		REG(GMBUS1) = (1 << 31);
		REG(GMBUS1) = 0;
		lil_assert(gmbus2_wait_for_mask(gpu, con, 0x200, true));
	}
}

static GmbusStatus gmbus_wait_progress(LilGpu *gpu, LilConnector *con) {
	size_t tries = 0;

	while(1) {
		uint32_t status = REG(GMBUS2);

		if(status & (GMBUS_NAK)) {
			REG(GMBUS1) = (1 << 31);
			REG(GMBUS1) = 0;
			lil_assert(gmbus2_wait_for_mask(gpu, con, 0x200, true));
			return GmbusStatus::Nak;
        }

        if(status & GMBUS_HW_RDY)
            return GmbusStatus::Success;

		if(tries++ > 150000)
			return GmbusStatus::Timeout;
	}
}

static GmbusStatus gmbus_wait_completion(LilGpu* gpu, LilConnector *con) {
	size_t tries = 0;

    while(1) {
        uint32_t status = REG(GMBUS2);

        if(status & (GMBUS_NAK)) {
            REG(GMBUS1) = (1 << 31);
			REG(GMBUS1) = 0;
			lil_assert(gmbus2_wait_for_mask(gpu, con, 0x200, true));
			return GmbusStatus::Nak;
        }

        if(status & GMBUS_HW_WAIT_PHASE)
            return GmbusStatus::Success;

		if(tries++ > 150000)
			return GmbusStatus::Timeout;
    }
}

static bool gmbus_transfer(LilGpu *gpu, LilConnector *con, uint8_t offset, uint8_t index, size_t len, uint8_t *buf, bool write) {
	size_t progress = 0;

	auto read_stream = [&] {
		uint32_t data = REG(GMBUS3);

        for(int i = 0; i < 4; i++) {
            if(progress == len)
                break;
            buf[progress++] |= data >> (8 * i);
        }
	};

	auto write_stream = [&] {
		uint32_t data = 0;
		for(size_t i = 0; i < 4; ++i) {
			if(progress == len)
				break;
			data |= uint32_t{buf[progress++]} << (8 * i);
		}
		REG(GMBUS3) = data;
	};

	size_t retries = 0;
	GmbusStatus status;

    uint32_t gmbus_command_val = REG(GMBUS1);
    gmbus_command_val = GMBUS_CYCLE_WAIT | GMBUS_CYCLE_INDEX |
                        GMBUS_LEN(len) | GMBUS_OFFSET(offset) | GMBUS_INDEX(index) |
                        GMBUS_SW_READY;

	if(!write)
		gmbus_command_val |= GMBUS_READ;
	else
		gmbus_command_val |= GMBUS_WRITE;

retry:
	gmbus_setup(gpu, con);

	if(write)
		write_stream();

    REG(GMBUS1) = gmbus_command_val;

	if(write)
		if(status = gmbus_wait_progress(gpu, con); status != GmbusStatus::Success) {
			if(status == GmbusStatus::Timeout)
				return false;

			goto handle_error;
		}

    while(progress < len) {
		if(!write) {
			if(status = gmbus_wait_progress(gpu, con); status != GmbusStatus::Success) {
				if(status == GmbusStatus::Timeout)
					return false;

				goto handle_error;
			}

			read_stream();
		} else {
			write_stream();

			if(status = gmbus_wait_progress(gpu, con); status != GmbusStatus::Success) {
				if(status == GmbusStatus::Timeout)
					return false;

				goto handle_error;
			}
		}
    }

	if(gmbus_wait_completion(gpu, con) == GmbusStatus::Nak || retries > 3) {
		return false;
	}

	return true;

handle_error:
	if(retries++ > 3)
		return false;

	if(!gmbus2_wait_for_mask(gpu, con, (1 << 9), true)) {
		return false;
	}

	REG(GMBUS1) = GMBUS_CLEAR_INTERRUPT;
    REG(GMBUS1) = 0;
    REG(GMBUS0) = 0;

	goto retry;
}

bool gmbus_read(LilGpu* gpu, LilConnector *con, uint8_t offset, uint8_t index, size_t len, uint8_t *buf) {
	LilEncoder *enc = con->encoder;
	uint8_t pin_pair = ddc_pin(con);

	lil_assert((pin_pair & ~7) == 0);

	return gmbus_transfer(gpu, con, offset, index, len, buf, false);
}

bool gmbus_write(LilGpu* gpu, LilConnector *con, uint8_t offset, uint8_t index, size_t len, uint8_t *buf) {
	LilEncoder *enc = con->encoder;
	uint8_t pin_pair = ddc_pin(con);
	size_t retries = 0;
	GmbusStatus status;

	lil_assert((pin_pair & ~7) == 0);
	return gmbus_transfer(gpu, con, offset, index, len, buf, true);
}

uint8_t lil_gmbus_get_mode_info(LilGpu* gpu, LilConnector *con, LilModeInfo* out) {
    DisplayData edid = {};
    gmbus_read(gpu, con, 0x50, 0, 128, (uint8_t*)&edid);

    int j = 0;
    for (int i = 0; i < 4; i++) {
        if(edid.detailTimings[i].pixelClock == 0)
            continue; // Not a timing descriptor

        edid_timing_to_mode(&edid, edid.detailTimings[i], &out[j++]);
    }

    return j;
}
