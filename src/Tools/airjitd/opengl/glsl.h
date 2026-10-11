#ifndef AIRJITD_GLSL_H
#define AIRJITD_GLSL_H

#include "../../air2spv/air2spv.h"
#include "../bridge.h"

struct pd_gl_pair { unsigned texture; int sampler; uint32_t flags; char name[64]; };
struct pd_gl_bindings { struct pd_gl_pair pair[64]; unsigned count; };

// Caller owns source. Buffer blocks and push members have stable names for
// binding after linking; no Vulkan descriptors or shader-storage extensions.
int pd_glsl_source(const uint32_t *words, size_t nwords, const struct a2s_reflect *r,
                   const struct a2s_reflect *fragment, unsigned stage, const uint32_t *samplers, struct pd_gl_bindings *bindings, const struct PDComputeRequest *kernel, char **source, char *error, size_t error_size);

#endif
