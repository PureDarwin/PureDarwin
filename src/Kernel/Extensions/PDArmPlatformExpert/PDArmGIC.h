#ifndef _PUREDARWIN_PDARMGIC_H
#define _PUREDARWIN_PDARMGIC_H

#include <IOKit/IOTypes.h>

bool PDArmGIC_init(void);
bool PDArmGIC_enable(void);
bool PDArmGIC_map_cpu(unsigned int cpu);
bool PDArmGIC_init_cpu(unsigned int cpu);
bool PDArmGIC_enable_cpu(unsigned int cpu);
void PDArmGIC_send_ipi(uint64_t target_mpidr);

#endif /* _PUREDARWIN_PDARMGIC_H */
