#ifndef AIRJITD_LLVM_AIR_H
#define AIRJITD_LLVM_AIR_H

#include <llvm-c/Core.h>

// set in flags when a lowering makes the stage need derivatives
#define AIR_USES_DERIV 1u

// gives the air.* declaration fn a body, 0 on success, negative when no lowering fits
int air_lower(LLVMModuleRef mod, LLVMValueRef fn, const char *name, unsigned *flags);

#endif
