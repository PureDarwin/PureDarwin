#ifndef AIRJITD_LLVM_AIR_H
#define AIRJITD_LLVM_AIR_H

#include <llvm-c/Core.h>

// set in flags when a lowering makes the stage need derivatives
#define AIR_USES_DERIV 1u
// set when it shuffles values between the lanes of a quad
#define AIR_USES_QUAD 2u
// set when it calls through an address of this process, which another run would not share
#define AIR_ABSOLUTE 4u
// set when a threadgroup barrier must wait for the group's other threads
#define AIR_USES_BARRIER 8u
// set when it samples with an implicit level of detail, which mipmapped textures take from derivatives
#define AIR_USES_LOD 16u

// replaces constexpr sampler globals with PDSamplers carrying their decoded flags
void air_const_samplers(LLVMModuleRef mod);

// gives the air.* declaration fn a body, 0 on success, negative when no lowering fits
int air_lower(LLVMModuleRef mod, LLVMValueRef fn, const char *name, unsigned *flags);

// runtime function i's symbol name and address, 0 past the last
int air_runtime(unsigned i, char *name, size_t len, void **impl);

#endif
