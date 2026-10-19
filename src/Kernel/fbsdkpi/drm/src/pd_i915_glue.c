// SPDX-License-Identifier: MIT
// i915 glue: stolen memory, freebsd and acpi stand-ins, stubs for left-out parts

#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/types.h>

#include <drm/intel/i915_drm.h>
#include <drm/intel/intel-gtt.h>

#include "i915_drv.h"
#include "gt/intel_ggtt_gmch.h"
#include "gt/intel_gt_debugfs.h"
#include "soc/intel_gmch.h"
#include "display/icl_dsi.h"
#include "display/intel_acpi.h"
#include "display/intel_dsi_dcs_backlight.h"
#include "display/intel_dvo.h"
#include "display/intel_lpe_audio.h"
#include "display/vlv_dsi.h"
#include "display/vlv_dsi_pll.h"

#include <drm/display/drm_hdcp_helper.h>

struct drm_dsc_picture_parameter_set;
struct mipi_dsi_device;

// the bios-reserved graphics memory, filled in from the gpu's config space before attach
struct resource intel_graphics_stolen_res;

void pd_i915_set_stolen(uint64_t base, uint64_t size);

void
pd_i915_set_stolen(uint64_t base, uint64_t size)
{
	intel_graphics_stolen_res = (struct resource)DEFINE_RES_MEM(base, size);
	printf("i915: stolen memory 0x%llx-0x%llx\n", (unsigned long long)base,
	    (unsigned long long)(base + size - 1));
}

// the bios maps MCHBAR on every platform PD drives, so i915 never has to place it
void *
bsd_intel_pci_bus_alloc_mem(device_t dev, int *rid, uintmax_t size,
    resource_size_t *start, resource_size_t *end)
{
	return NULL;
}

void
bsd_intel_pci_bus_release_mem(device_t dev, int rid, void *res)
{
}

// the gmch gtt belongs to gen2 to gen5
int
intel_ggtt_gmch_probe(struct i915_ggtt *ggtt)
{
	return -ENODEV;
}

int
intel_ggtt_gmch_enable_hw(struct drm_i915_private *i915)
{
	return -ENODEV;
}

void
intel_ggtt_gmch_flush(void)
{
}

// no acpi video or _DSM handling yet, the bios settings stand
void
intel_register_dsm_handler(void)
{
}

void
intel_unregister_dsm_handler(void)
{
}

void
intel_dsm_get_bios_data_funcs_supported(struct intel_display *display)
{
}

void
intel_acpi_device_id_update(struct intel_display *display)
{
}

void
intel_acpi_assign_connector_fwnodes(struct intel_display *display)
{
}

void
intel_acpi_video_register(struct intel_display *display)
{
}

// DSI panels are not driven yet, no DSI encoder is ever created
void
icl_dsi_init(struct intel_display *display,
    const struct intel_bios_encoder_data *devdata)
{
}

void
icl_dsi_frame_update(struct intel_crtc_state *crtc_state)
{
}

void
vlv_dsi_init(struct drm_i915_private *dev_priv)
{
}

int
vlv_dsi_min_cdclk(const struct intel_crtc_state *crtc_state)
{
	return 0;
}

void
vlv_dsi_wait_for_fifo_empty(struct intel_dsi *intel_dsi, enum port port)
{
}

bool
bxt_dsi_pll_is_enabled(struct drm_i915_private *dev_priv)
{
	return false;
}

void
assert_dsi_pll_enabled(struct drm_i915_private *i915)
{
}

void
assert_dsi_pll_disabled(struct drm_i915_private *i915)
{
}

int
intel_dsi_dcs_init_backlight_funcs(struct intel_connector *intel_connector)
{
	return -ENODEV;
}

ssize_t
mipi_dsi_picture_parameter_set(struct mipi_dsi_device *dsi,
    const struct drm_dsc_picture_parameter_set *pps)
{
	return -ENODEV;
}

ssize_t
mipi_dsi_compression_mode(struct mipi_dsi_device *dsi, bool enable)
{
	return -ENODEV;
}

// dvo encoders sit on gen2 to gen4
void
intel_dvo_init(struct drm_i915_private *dev_priv)
{
}

// lpe audio is the atom-only hdmi audio path
int
intel_lpe_audio_init(struct drm_i915_private *dev_priv)
{
	return -ENODEV;
}

void
intel_lpe_audio_teardown(struct drm_i915_private *dev_priv)
{
}

void
intel_lpe_audio_irq_handler(struct drm_i915_private *dev_priv)
{
}

void
intel_lpe_audio_notify(struct drm_i915_private *dev_priv,
    enum transcoder cpu_transcoder, enum port port, const void *eld,
    int ls_clock, bool dp_output)
{
}

// no debugfs on PureDarwin
void
intel_gt_debugfs_register(struct intel_gt *gt)
{
}

void
intel_gt_debugfs_register_files(struct dentry *root,
    const struct intel_gt_debugfs_file *files, unsigned long count, void *data)
{
}

// hdcp is not offered, so there is no revocation list to check against
int
drm_hdcp_check_ksvs_revoked(struct drm_device *dev, u8 *ksvs, u32 ksv_count)
{
	return 0;
}

// pdi915_selftest=1: builds a scatterlist the way i915's internal objects do,
// walks it like i915_gem_object_map_page and vmaps the pages, before i915 probes
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>
#include "i915_scatterlist.h"

long pd_drm_tunable_long(const char *path, long def);
void pd_i915_selftest(void);

static void
pd_i915_selftest_print(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	uint64_t addr = 0x7c000000;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	printf("i915 selftest: %pV, err %pe, addr %pa, %5.2s|%-4d|\n", &vaf, ERR_PTR(-22), &addr, "abc", 7);
	va_end(args);
}

void
pd_i915_selftest(void)
{
	const unsigned int order = 4, n = 1U << order;
	struct page *pages[1U << 4] = { };
	struct sg_table st;
	struct sgt_iter iter;
	struct page *page, *first;
	unsigned int i = 0, missing = 0;
	u32 *va;

	// drm_debug=N sets drm's debug categories (0x2 driver, 0x4 kms) before i915 probes
	extern unsigned long __drm_debug;
	__drm_debug = (unsigned long)pd_drm_tunable_long("drm.debug", (long)__drm_debug);
	// i915_<param>=<value> boot-args, before i915 copies its params at probe
	extern void pd_i915_params_from_bootargs(void);
	extern void pd_i915_display_params_from_bootargs(void);
	pd_i915_params_from_bootargs();
	pd_i915_display_params_from_bootargs();

	if (pd_drm_tunable_long("i915.selftest", 0) == 0)
		return;
	pd_i915_selftest_print("nested %s %d 0x%llx %zu", "message", -5, 0x915ULL, (size_t)42);
	first = alloc_pages(GFP_KERNEL, order);
	if (first == NULL || sg_alloc_table(&st, n, GFP_KERNEL) != 0) {
		printf("i915 selftest: allocation failed\n");
		return;
	}
	st.nents = 1;
	sg_set_page(st.sgl, first, PAGE_SIZE << order, 0);
	sg_mark_end(st.sgl);
	for_each_sgt_page(page, iter, &st) {
		if (i < n)
			pages[i] = page;
		i++;
	}
	for (unsigned int j = 0; j < n; j++) {
		if (pages[j] == NULL)
			missing++;
	}
	printf("i915 selftest: %u pages walked, %u missing, first pfn 0x%lx\n", i, missing,
	    (unsigned long)page_to_pfn(first));
	iter = __sgt_iter(st.sgl, false);
	printf("i915 selftest: sg_page %p first %p, iter pfn 0x%lx curr %u max %u, lookup %p %p\n",
	    sg_page(st.sgl), first, (unsigned long)iter.pfn, iter.curr, iter.max,
	    PHYS_TO_VM_PAGE(first->phys_addr), pfn_to_page(page_to_pfn(first) + 1));
	if (missing == 0) {
		va = vmap(pages, n, 0, PAGE_KERNEL);
		if (va != NULL) {
			va[0] = 0x915;
			va[n * PAGE_SIZE / 4 - 1] = 0x915;
			printf("i915 selftest: vmap ok at %p\n", va);
			vunmap(va);
		} else
			printf("i915 selftest: vmap failed\n");
	}
	sg_free_table(&st);
	__free_pages(first, order);

	// two chunks (16 + 2 pages) mapped write-combined, like gen7's clear batch
	{
		struct page *a = alloc_pages(GFP_KERNEL, 4), *b = alloc_pages(GFP_KERNEL, 1);
		struct page *pp[18] = { };
		unsigned int k = 0;
		u32 *w;

		if (a == NULL || b == NULL || sg_alloc_table(&st, 18, GFP_KERNEL) != 0) {
			printf("i915 selftest: wc allocation failed\n");
			return;
		}
		st.nents = 2;
		sg_set_page(st.sgl, a, PAGE_SIZE << 4, 0);
		sg_set_page(sg_next(st.sgl), b, PAGE_SIZE << 1, 0);
		sg_mark_end(sg_next(st.sgl));
		for_each_sgt_page(page, iter, &st) {
			if (k < 18)
				pp[k] = page;
			k++;
		}
		printf("i915 selftest: two chunks walked %u pages\n", k);
		w = vmap(pp, 18, 0, pgprot_writecombine(PAGE_KERNEL));
		if (w != NULL) {
			for (unsigned int j = 0; j < 18; j++) {
				printf("i915 selftest: wc page %u phys 0x%llx\n", j,
				    (unsigned long long)page_to_phys(pp[j]));
				w[j * PAGE_SIZE / 4] = j;
			}
			printf("i915 selftest: wc vmap of 18 pages ok\n");
			vunmap(w);
		}
		sg_free_table(&st);
		__free_pages(a, 4);
		__free_pages(b, 1);
	}
}
