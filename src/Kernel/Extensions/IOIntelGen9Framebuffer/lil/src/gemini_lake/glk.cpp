#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/gemini_lake/dp.hpp"
#include "src/gemini_lake/glk.hpp"
#include "src/gemini_lake/hdmi.hpp"
#include "src/gemini_lake/phy.hpp"
#include "src/kaby_lake/dp.hpp"
#include "src/kaby_lake/gtt.hpp"
#include "src/kaby_lake/pci.hpp"
#include "src/kaby_lake/pcode.hpp"
#include "src/kaby_lake/setup.hpp"
#include "src/kaby_lake/transcoder.hpp"
#include "src/regs.hpp"
#include "src/vbt/vbt.hpp"

namespace glk {

void init_gpu(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	gpu->vmem_clear = kbl::gtt::vmem_clear;
	gpu->vmem_map = kbl::gtt::vmem_map;

	gpu->max_connectors = 4;
	gpu->connectors = reinterpret_cast<LilConnector *>(lil_malloc(sizeof(LilConnector) * gpu->max_connectors));

	// Shared gen9 plumbing: identify the part, map BARs, bring up power wells /
	// CD clock (initialize_display branches to the GLK sequence internally).
	kbl::pci::detect(gpu);
	kbl::setup::setup(gpu);
	kbl::setup::initialize_display(gpu);
	kbl::setup::hotplug_enable(gpu);
	kbl::setup::psr_disable(gpu);

	// Start from all transcoders disabled.
	LilTranscoder transcoders[3] = { TRANSCODER_A, TRANSCODER_B, TRANSCODER_C };
	for(auto t : transcoders) {
		kbl::transcoder::disable(gpu, t);
		kbl::transcoder::ddi_disable(gpu, t);
		kbl::transcoder::clock_disable_by_id(gpu, t);
	}

	// Read the memory latency levels used for plane watermark programming.
	// Without valid watermarks the plane FIFO underruns when it is enabled.
	uint32_t data0 = 0, data1 = 0, timeout = 100;
	if(kbl::pcode::rw(gpu, &data0, &data1, kbl::pcode::Mailbox::GEN9_READ_MEM_LATENCY, &timeout)) {
		gpu->mem_latency_first_set = data0;
		data0 = 1; data1 = 0; timeout = 100;
		if(kbl::pcode::rw(gpu, &data0, &data1, kbl::pcode::Mailbox::GEN9_READ_MEM_LATENCY, &timeout))
			gpu->mem_latency_second_set = data0;
	}

	// Enumerate connectors from the VBT (gen9-generic; assigns the gmbus/EDID
	// based get_connector_info/is_connected callbacks).
	vbt_setup_children(gpu);

	// Initialize the DPIO PHY for each connector's DDI and route HDMI connectors
	// to the Broxton mode-set path.
	for(size_t i = 0; i < gpu->num_connectors; i++) {
		LilConnector *con = &gpu->connectors[i];

		glk::phy::init(gpu, con->ddi_id);

		// The reused gen9 pipe/DDB/timing/watermark code only implements pipe A.
		// VBT may assign the connector to another pipe; for a single display,
		// drive it on pipe A / transcoder A instead (any DDI can feed it).
		auto route_pipe_a = [&]() {
			con->crtc->pipe_id = 0;
			con->crtc->transcoder = TRANSCODER_A;
			for(uint32_t p = 0; p < con->crtc->num_planes; p++)
				con->crtc->planes[p].pipe_id = 0;
		};

		if(con->type == HDMI) {
			con->crtc->commit_modeset = glk::hdmi::commit_modeset;
			con->crtc->shutdown = glk::hdmi::shutdown;
			route_pipe_a();
		} else if(con->type == DISPLAYPORT) {
			// Probe the port: real DP sink vs a passive DP++ -> HDMI adapter.
			switch(glk::dp::pre_enable(gpu, con)) {
				case glk::dp::SinkKind::DisplayPort:
					con->crtc->commit_modeset = glk::dp::commit_modeset;
					con->crtc->shutdown = glk::dp::shutdown;
					route_pipe_a();
					break;
				case glk::dp::SinkKind::DualModeHDMI:
					// Drive the DP port as HDMI/TMDS (dual mode). Treat it as an
					// HDMI connector so the mode set selects HDMI signaling.
					con->type = HDMI;
					con->crtc->commit_modeset = glk::hdmi::commit_modeset;
					con->crtc->shutdown = glk::hdmi::shutdown;
					route_pipe_a();
					break;
				case glk::dp::SinkKind::None:
					lil_log(WARNING, "glk: no sink on DP connector %lu\n", i);
					break;
			}
		}

		lil_log(INFO, "glk: connector %lu type %u on DDI %c ready\n",
		        i, con->type, '0' + con->ddi_id);
	}
}

} // namespace glk
