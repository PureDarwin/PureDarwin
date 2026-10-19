// SPDX-License-Identifier: MIT
// arm dma iommu mappings, only nouveau's tegra path asks for them
#ifndef _PD_ASM_DMA_IOMMU_H_
#define _PD_ASM_DMA_IOMMU_H_

struct dma_iommu_mapping;
struct device;

static inline struct dma_iommu_mapping *to_dma_iommu_mapping(struct device *dev) { return 0; }
static inline void arm_iommu_detach_device(struct device *dev) { }
static inline void arm_iommu_release_mapping(struct dma_iommu_mapping *m) { }

#endif
