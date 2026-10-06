#ifndef _PUREDARWIN_PDARMGIC_H
#define _PUREDARWIN_PDARMGIC_H

#include <IOKit/IOTypes.h>

bool PDArmGIC_init(void);
bool PDArmGIC_enable(void);
bool PDArmGIC_map_cpu(unsigned int cpu, uint64_t mpidr);
bool PDArmGIC_init_cpu(unsigned int cpu);
bool PDArmGIC_enable_cpu(unsigned int cpu);
void PDArmGIC_send_ipi(uint64_t target_mpidr);
// the gicv3 distributor this kext drives, NULL on a gic-400 or a gicv2
volatile uint8_t *PDArmGIC_v3_distributor(void);
volatile uint8_t *PDArmGIC_v2_distributor(void);

#endif /* _PUREDARWIN_PDARMGIC_H */
