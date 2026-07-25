#include <lil/intel.h>
#include <lil/imports.h>
#include <lil/vbt.h>
#include <lil/vbt-types.h>

#include "src/base.hpp"
#include "src/debug.hpp"
#include "src/kaby_lake/encoder.hpp"
#include "src/kaby_lake/hdmi.hpp"
#include "src/kaby_lake/plane.hpp"
#include "src/kaby_lake/dp.hpp"
#include "src/pci.hpp"
#include "src/vbt/opregion.hpp"
#include "src/vbt/vbt.hpp"

//
// TODO: vbt parsing should be generation independent
//

const struct vbt_header *lil_vbt_locate(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	auto asls_phys = gpu->pci_read<uint32_t>(PCI_ASLS);
	if(!asls_phys) {
		lil_panic("ACPI OpRegion not supported");
	}

	auto opregion = reinterpret_cast<struct opregion_header *>(lil_map(asls_phys, 0x2000));
	if(memcmp(opregion, OPREGION_SIGNATURE, 16)) {
		lil_panic("ACPI OpRegion signature mismatch");
	}

	lil_log(VERBOSE, "ACPI OpRegion %u.%u.%u\n", opregion->over.major, opregion->over.minor, opregion->over.revision);
	opregion_asle *asle = nullptr;

	if(opregion->mbox & MBOX_ASLE)
		asle = reinterpret_cast<opregion_asle *>(uintptr_t(opregion) + OPREGION_ASLE_OFFSET);

	if(opregion->over.major >= 2 && asle && asle->rvda && asle->rvds) {
		uint64_t rvda = asle->rvda;
		lil_log(VERBOSE, "Raw RVDA in asle->rvda: 0x%lx\n", asle->rvda);

		// In OpRegion v2.1+, rvda was changed to a relative offset
		if(opregion->over.major > 2 || (opregion->over.major == 2 && opregion->over.minor >= 1)) {
			if(rvda < OPREGION_SIZE) {
				lil_log(WARNING, "VBT base shouldn't be within OpRegion, but it is!\n");
			}

			rvda += asls_phys;
		}

		lil_log(INFO, "VBT RVDA: 0x%lx\n", rvda);

		/* OpRegion 2.0: rvda is a physical address */
		void *vbt = lil_map(rvda, asle->rvds);

		const struct vbt_header *vbt_header = vbt_get_header(vbt, asle->rvds);
		if(!vbt_header) {
			lil_log(WARNING, "VBT header from ACPI OpRegion ASLE is invalid!\n");
			lil_unmap(vbt, asle->rvds);
		} else {
			return vbt_header;
		}
	}

	if(!(opregion->mbox & MBOX_VBT)) {
		lil_log(ERROR, "ACPI OpRegion does not support VBT mailbox when it should!\n");
		lil_panic("Invalid ACPI OpRegion");
	}

	size_t vbt_size = ((opregion->mbox & MBOX_ASLE_EXT) ? OPREGION_ASLE_EXT_OFFSET : OPREGION_SIZE) - OPREGION_VBT_OFFSET;
	void *vbt = lil_map(asls_phys + OPREGION_VBT_OFFSET, vbt_size);

	const struct vbt_header *vbt_header = vbt_get_header(vbt, vbt_size);

	if(!vbt_header) {
		lil_log(ERROR, "Reading VBT from ACPI OpRegion mailbox 4 failed!\n");
		lil_panic("No supported method to obtain VBT left");
	}

	return vbt_header;
}

void vbt_init(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	gpu->vbt_header = lil_vbt_locate(lil_gpu);
	lil_log(VERBOSE, "lil: gpu->vbt_header addr 0x%lx\n", (uintptr_t) gpu->vbt_header);

	const struct bdb_header *bdb_hdr = vbt_get_bdb_header(gpu->vbt_header);
	if(!bdb_hdr)
		lil_panic("BDB header not found");

	auto driver_features_block = vbt_get_bdb_block<bdb_driver_features>(gpu->vbt_header);
	if(!driver_features_block)
		lil_panic("BDB driver features not found");

	auto general_defs = vbt_get_bdb_block<bdb_general_definitions>(gpu->vbt_header);
	if(!general_defs)
		lil_panic("BDB general definitions not found");
}

namespace {

uint32_t vbt_handle_to_port(uint32_t handle) {
	switch(handle) {
		case 4:
			return 0;
		case 0x10:
			return 3;
		case 0x20:
			return 2;
		case 0x40:
			return 1;
		default: {
			lil_log(ERROR, "unexpected VBT child handle %#x\n", handle);
			lil_panic("unhandled VBT child handle");
		}
	}
}

enum LilAuxChannel vbt_parse_aux_channel(uint8_t aux_ch) {
	switch(aux_ch) {
		case 0x40: return AUX_CH_A;
		case 0x10: return AUX_CH_B;
		case 0x20: return AUX_CH_C;
		case 0x30: return AUX_CH_D;
		case 0x50: return AUX_CH_E;
		case 0x60: return AUX_CH_F;
		case 0x70: return AUX_CH_G;
		case 0x80: return AUX_CH_H;
		case 0x90: return AUX_CH_I;
		default: {
			lil_log(WARNING, "Unhandled VBT child AUX channel %#x; falling back to AUX channel A\n", aux_ch);
			return AUX_CH_A;
		}
	}
}

} // namespace

void vbt_setup_children(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);
	size_t con_id = 0;

	const struct bdb_driver_features *driver_features = vbt_get_bdb_block<bdb_driver_features>(gpu->vbt_header);
	const struct bdb_general_definitions *general_defs = vbt_get_bdb_block<bdb_general_definitions>(gpu->vbt_header);

	// VBT may advertise an eDP (lvds_config != NONE) yet leave the first child
	// device empty (device_type 0) on boards with no physical internal panel -
	// common on Gemini Lake mini-PCs driven over HDMI/DP. Only claim child[0] as
	// the eDP when it is actually an internal/DisplayPort connector; otherwise fall
	// through to the general child loop so the real HDMI/DP connectors enumerate.
	if(driver_features->lvds_config != LVDS_CONFIG_NONE && !skipEmbeddedDisplayPort
	   && (reinterpret_cast<const child_device *>(&general_defs->child_dev)->device_type
	       & (DEVICE_TYPE_DISPLAYPORT_OUTPUT | DEVICE_TYPE_INTERNAL_CONNECTOR))) {
		auto dev = reinterpret_cast<const child_device *>(&general_defs->child_dev);
		LilConnector *con = &gpu->connectors[0];

		con->id = 0;
		con->type = EDP;
		con->on_pch = true;
		con->ddi_id = vbt_dvo_to_ddi(dev->dvo_port);
		lil_assert(con->ddi_id == 0 || con->ddi_id == 3);
		con->aux_ch = vbt_parse_aux_channel(dev->aux_channel);

		con->get_connector_info = kbl::dp::get_connector_info;
		con->is_connected = kbl::dp::is_connected;

		con->encoder = reinterpret_cast<LilEncoder *>(lil_malloc(sizeof(LilEncoder)));

		con->crtc = reinterpret_cast<LilCrtc *>(lil_malloc(sizeof(LilCrtc)));
		con->crtc->transcoder = (con->ddi_id == DDI_A) ? TRANSCODER_EDP : TRANSCODER_A;
		con->crtc->connector = &gpu->connectors[0];
		con->crtc->pipe_id = 0;
		con->crtc->commit_modeset = kbl::dp::commit_modeset;
		con->crtc->shutdown = kbl::dp::crtc_shutdown;

		con->crtc->num_planes = 1;
		con->crtc->planes = reinterpret_cast<LilPlane *>(lil_malloc(sizeof(LilPlane)));
		for(size_t i = 0; i < con->crtc->num_planes; i++) {
			con->crtc->planes[i].enabled = 0;
			con->crtc->planes[i].pipe_id = con->crtc->pipe_id;
			con->crtc->planes[i].update_surface = kbl::plane::update_primary_surface;
			con->crtc->planes[i].get_formats = kbl::plane::get_formats;
		}

		kbl::encoder::edp_init(gpu, con->encoder);

		con_id++;
	}

	size_t children = (general_defs->size - sizeof(*general_defs) + sizeof(struct bdb_block_header)) / general_defs->child_dev_size;

	for(size_t child = con_id; child < children; child++) {
		auto dev = reinterpret_cast<struct child_device *>(uintptr_t(&general_defs->child_dev) + (child * general_defs->child_dev_size));
		uint32_t device_type = dev->device_type;

		switch(device_type) {
			case 0: {
				continue;
			}
			case DEVICE_TYPE_DP: {
				LilConnector *con = &gpu->connectors[con_id];

				con->id = vbt_handle_to_port(dev->handle);
				con->type = DISPLAYPORT;
				con->ddi_id = vbt_dvo_to_ddi(dev->dvo_port);
				con->aux_ch = vbt_parse_aux_channel(dev->aux_channel);

				con->get_connector_info = kbl::dp::get_connector_info;
				con->is_connected = kbl::dp::is_connected;

				con->encoder = reinterpret_cast<LilEncoder *>(lil_malloc(sizeof(LilEncoder)));

				con->crtc = reinterpret_cast<LilCrtc *>(lil_malloc(sizeof(LilCrtc)));
				con->crtc->connector = con;

				// TODO(CLEAN):	not optimal
				// 				on gemini lake this seems to always be 0
				con->crtc->pipe_id = 0;
				con->crtc->transcoder = static_cast<LilTranscoder>(con->crtc->pipe_id);
				con->crtc->commit_modeset = kbl::dp::commit_modeset;
				con->crtc->shutdown = kbl::dp::crtc_shutdown;

				con->crtc->num_planes = 1;
				con->crtc->planes = reinterpret_cast<LilPlane *>(lil_malloc(sizeof(LilPlane)));
				for(size_t i = 0; i < con->crtc->num_planes; i++) {
					con->crtc->planes[i].enabled = true;
					con->crtc->planes[i].pipe_id = con->crtc->pipe_id;
					con->crtc->planes[i].update_surface = kbl::plane::update_primary_surface;
					con->crtc->planes[i].get_formats = kbl::plane::get_formats;
				}

				kbl::encoder::dp_init(gpu, con->encoder, dev);

				con_id++;
				break;
			}

			case DEVICE_TYPE_HDMI: {
				LilConnector *con = &gpu->connectors[con_id];

				con->id = vbt_handle_to_port(dev->handle);
				con->type = HDMI;
				con->ddi_id = vbt_dvo_to_ddi(dev->dvo_port);
				con->aux_ch = vbt_parse_aux_channel(dev->aux_channel);

				con->get_connector_info = kbl::hdmi::get_connector_info;
				con->is_connected = kbl::hdmi::is_connected;

				con->encoder = reinterpret_cast<LilEncoder *>(lil_malloc(sizeof(LilEncoder)));

				con->crtc = reinterpret_cast<LilCrtc *>(lil_malloc(sizeof(LilCrtc)));
				con->crtc->connector = con;

				con->crtc->pipe_id = 1;
				con->crtc->transcoder = static_cast<LilTranscoder>(con->crtc->pipe_id);
				con->crtc->commit_modeset = kbl::hdmi::commit_modeset;
				con->crtc->shutdown = kbl::hdmi::shutdown;

				con->crtc->num_planes = 1;
				con->crtc->planes = reinterpret_cast<LilPlane *>(lil_malloc(sizeof(LilPlane)));
				for(size_t i = 0; i < con->crtc->num_planes; i++) {
					con->crtc->planes[i].enabled = true;
					con->crtc->planes[i].pipe_id = con->crtc->pipe_id;
					con->crtc->planes[i].update_surface = kbl::plane::update_primary_surface;
					con->crtc->planes[i].get_formats = kbl::plane::get_formats;
				}

				kbl::encoder::hdmi_init(gpu, con->encoder, dev);

				con_id++;
				break;
			}

			case DEVICE_TYPE_DP_DUAL_MODE: {
				LilConnector *con = &gpu->connectors[con_id];

				con->id = vbt_handle_to_port(dev->handle);
				// TODO: don't hardcode this to HDMI
				con->type = HDMI;
				con->ddi_id = vbt_dvo_to_ddi(dev->dvo_port);
				con->aux_ch = vbt_parse_aux_channel(dev->aux_channel);

				con->get_connector_info = kbl::hdmi::get_connector_info;
				con->is_connected = kbl::hdmi::is_connected;

				con->encoder = reinterpret_cast<LilEncoder *>(lil_malloc(sizeof(LilEncoder)));

				con->crtc = reinterpret_cast<LilCrtc *>(lil_malloc(sizeof(LilCrtc)));
				con->crtc->connector = con;

				con->crtc->pipe_id = 1;
				con->crtc->transcoder = static_cast<LilTranscoder>(con->crtc->pipe_id);
				con->crtc->commit_modeset = kbl::hdmi::commit_modeset;
				con->crtc->shutdown = kbl::hdmi::shutdown;

				con->crtc->num_planes = 1;
				con->crtc->planes = reinterpret_cast<LilPlane *>(lil_malloc(sizeof(LilPlane)));
				for(size_t i = 0; i < con->crtc->num_planes; i++) {
					con->crtc->planes[i].enabled = true;
					con->crtc->planes[i].pipe_id = con->crtc->pipe_id;
					con->crtc->planes[i].update_surface = kbl::plane::update_primary_surface;
					con->crtc->planes[i].get_formats = kbl::plane::get_formats;
				}

				kbl::encoder::hdmi_init(gpu, con->encoder, dev);

				con_id++;
				break;
			}
			default: {
				lil_log(WARNING, "VBT: found unknown device type %#x\n", device_type);
				break;
			}
		}
	}

	gpu->num_connectors = con_id;
}
