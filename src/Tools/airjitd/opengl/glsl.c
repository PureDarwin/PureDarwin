#include "glsl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spirv_cross/spirv_cross_c.h>

static char *replace_word(const char *source, const char *word, const char *replacement) {
  size_t count = 0, length;
  const char *at = source;
  char *result, *out;

  while ((at = strstr(at, word))) {
    count++;
    at += strlen(word);
  }

  length = strlen(source) + count * (strlen(replacement) - strlen(word)) + 1;
  result = malloc(length), out = result;
  if (!result)
    return 0;

  while ((at = strstr(source, word))) {
    memcpy(out, source, at - source);
    out += at - source;
    strcpy(out, replacement);
    out += strlen(replacement);
    source = at + strlen(word);
  }

  strcpy(out, source);
  return result;
}

int pd_glsl_source(const uint32_t *words, size_t nwords, const struct a2s_reflect *r,
                   const struct a2s_reflect *fragment, unsigned stage, const uint32_t *samplers,
                   struct pd_gl_bindings *bindings, const struct PDComputeRequest *kernel, char **source, char *error,
                   size_t error_size) {
  spvc_context context = 0;
  spvc_parsed_ir ir;
  spvc_compiler compiler;
  spvc_compiler_options options;
  spvc_resources resources;
  const spvc_reflected_resource *list;
  size_t count;
  const char *text;
  char name[96];
  int rc = -1;
  const spvc_specialization_constant *spec;
  size_t nspec;
  spvc_variable_id dummy;
  const spvc_combined_image_sampler *pairs;
  size_t npairs;
  char *adjusted = 0;
  const char *block = "#ifdef GL_ARB_shader_draw_parameters\n#define SPIRV_Cross_BaseInstance gl_BaseInstanceARB\n#else\nuniform int SPIRV_Cross_BaseInstance;\n#endif";
  const char *found;
  const char *line;
  const char *origin = "layout(origin_upper_left) in vec4 gl_FragCoord;\n";

  *source = 0;
  memset(bindings, 0, sizeof(*bindings));
  if (spvc_context_create(&context) != SPVC_SUCCESS) {
    snprintf(error, error_size, "cannot create GLSL compiler");
    return -1;
  }

#define CROSS(call) \
  do { \
    if ((call) != SPVC_SUCCESS) \
      goto done; \
  } while (0)
  CROSS(spvc_context_parse_spirv(context, words, nwords, &ir));
  CROSS(spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));
  CROSS(spvc_compiler_create_compiler_options(compiler, &options));
  CROSS(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, kernel ? 430 : 150));
  CROSS(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE));
  CROSS(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_FALSE));
  CROSS(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, SPVC_FALSE));
  CROSS(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_FIXUP_DEPTH_CONVENTION, SPVC_TRUE));
  CROSS(spvc_compiler_install_compiler_options(compiler, options));
  CROSS(spvc_compiler_create_shader_resources(compiler, &resources));

  // glsl 1.50 links varyings by name and spir-v links them by location
  CROSS(spvc_resources_get_resource_list_for_type(
      resources, stage ? SPVC_RESOURCE_TYPE_STAGE_INPUT : SPVC_RESOURCE_TYPE_STAGE_OUTPUT, &list, &count));
  for (size_t i = 0; i < count; i++) {
    unsigned location = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationLocation);

    snprintf(name, sizeof(name), "pd_varying%u", location);
    spvc_compiler_set_name(compiler, list[i].id, name);

    // opengl wants interpolation qualifiers on both sides of the link
    if (!stage && fragment) {
      for (unsigned k = 0; k < fragment->nvar; k++) {
        if (fragment->var[k].location != location)
          continue;

        if (fragment->var[k].interp == 1)
          spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationNoPerspective, 0);
        if (fragment->var[k].interp == 2)
          spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationFlat, 0);
      }
    }
  }

  if (stage == 0) {
    CROSS(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, &list, &count));
    for (size_t i = 0; i < count; i++) {
      unsigned location = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationLocation);

      snprintf(name, sizeof(name), "pd_attribute%u", location);
      spvc_compiler_set_name(compiler, list[i].id, name);
    }
  }

  if (stage == 1) {
    CROSS(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, &list, &count));
    for (size_t i = 0; i < count; i++) {
      unsigned location = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationLocation);

      snprintf(name, sizeof(name), "pd_color%u", location);
      spvc_compiler_set_name(compiler, list[i].id, name);
    }
  }

  CROSS(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count));
  for (size_t i = 0; i < count; i++) {
    unsigned binding = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);

    snprintf(name, sizeof(name), "PDStage%uBuffer%u", stage, binding);
    spvc_compiler_set_name(compiler, list[i].base_type_id, name);
    snprintf(name, sizeof(name), "pd_stage%u_buffer%u", stage, binding);
    spvc_compiler_set_name(compiler, list[i].id, name);
    spvc_compiler_unset_decoration(compiler, list[i].id, SpvDecorationDescriptorSet);
    spvc_compiler_unset_decoration(compiler, list[i].id, SpvDecorationBinding);
  }

  CROSS(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &list, &count));
  for (size_t i = 0; i < count; i++) {
    unsigned binding = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);

    snprintf(name, sizeof(name), "PDStage%uBuffer%u", stage, binding);
    spvc_compiler_set_name(compiler, list[i].base_type_id, name);
    spvc_compiler_unset_decoration(compiler, list[i].id, SpvDecorationDescriptorSet);
    spvc_compiler_unset_decoration(compiler, list[i].id, SpvDecorationBinding);
  }

  CROSS(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &list, &count));
  for (size_t i = 0; i < count; i++) {
    snprintf(name, sizeof(name), "pd_stage%u_push", stage);
    spvc_compiler_set_name(compiler, list[i].id, name);
    for (unsigned k = 0; k < r->npc; k++) {
      snprintf(name, sizeof(name), "word%u", k);
      spvc_compiler_set_member_name(compiler, list[i].base_type_id, k, name);
    }
  }

  // resolve separate metal texture and sampler bindings into glsl combined samplers
  CROSS(spvc_compiler_get_specialization_constants(compiler, &spec, &nspec));
  for (size_t i = 0; i < nspec; i++) {
    unsigned id = spec[i].constant_id;

    if (kernel && (id < 3 || (id >= A2S_SPEC_PHYS_SIZE && id < A2S_SPEC_PHYS_SIZE + 3) ||
                   (id >= A2S_SPEC_TGMEM && id < A2S_SPEC_TGMEM + 4))) {
      unsigned value = id < 3                ? kernel->tpg[id] :
                       id < A2S_SPEC_TGMEM ? kernel->tpg[id - A2S_SPEC_PHYS_SIZE] :
                                               kernel->tgmem[id - A2S_SPEC_TGMEM] / 4;

      spvc_constant_set_scalar_u32(spvc_compiler_get_constant_handle(compiler, spec[i].id), 0, 0, value);
    }

    if (id >= A2S_SPEC_SAMPLER_PIXEL && id < A2S_SPEC_SAMPLER_PIXEL + 16) {
      uint32_t flags = samplers ? samplers[id - A2S_SPEC_SAMPLER_PIXEL] : 0;

      spvc_constant_set_scalar_u32(spvc_compiler_get_constant_handle(compiler, spec[i].id), 0, 0,
                                   flags && !(flags & 0x80000000u));
    }
  }

  CROSS(spvc_compiler_build_dummy_sampler_for_combined_images(compiler, &dummy));
  CROSS(spvc_compiler_build_combined_image_samplers(compiler));
  CROSS(spvc_compiler_get_combined_image_samplers(compiler, &pairs, &npairs));
  if (npairs > 64)
    goto done;

  for (size_t i = 0; i < npairs; i++) {
    unsigned tb = spvc_compiler_get_decoration(compiler, pairs[i].image_id, SpvDecorationBinding);
    unsigned sb = spvc_compiler_get_decoration(compiler, pairs[i].sampler_id, SpvDecorationBinding);
    struct pd_gl_pair *p = &bindings->pair[bindings->count++];

    p->texture = ~0u, p->sampler = -1;
    p->flags = 0xc0000000u;
    for (unsigned k = 0; k < r->nres; k++) {
      const struct a2s_resource *res = &r->res[k];

      if (res->kind == A2S_RES_TEXTURE && res->binding == tb)
        p->texture = res->metal;

      if (pairs[i].sampler_id != dummy && res->binding == sb) {
        if (res->kind == A2S_RES_SAMPLER) {
          p->sampler = res->metal;
          p->flags = samplers ? samplers[res->metal] : 0;
        } else if (res->kind == A2S_RES_CONST_SAMPLER) {
          struct a2s_sampler_desc d;

          p->flags = a2s_sampler_flags(res->state);
          a2s_sampler_state(res->state, &d);
          p->flags |= d.mip << 4;
        }
      }
    }

    snprintf(p->name, sizeof(p->name), "pd_stage%u_texture%zu", stage, i);
    spvc_compiler_set_name(compiler, pairs[i].combined_id, p->name);
    spvc_compiler_unset_decoration(compiler, pairs[i].combined_id, SpvDecorationDescriptorSet);
    spvc_compiler_unset_decoration(compiler, pairs[i].combined_id, SpvDecorationBinding);
  }

  CROSS(spvc_compiler_compile(compiler, &text));

  // core 3.2 draws emulate base-instance through the fallback uniform
  // force that spelling even when the host has shader_draw_parameters
  found = stage == 0 ? strstr(text, block) : 0;
  if (found) {
    const char *replacement = "uniform int SPIRV_Cross_BaseInstance;";
    size_t prefix = found - text;

    adjusted = malloc(strlen(text) + 1);
    if (!adjusted)
      goto done;

    memcpy(adjusted, text, prefix);
    strcpy(adjusted + prefix, replacement);
    strcpy(adjusted + prefix + strlen(replacement), found + strlen(block));
    text = adjusted;
  }

  // metal fragment positions use the target's upper-left origin, glsl defaults to lower-left
  line = strchr(text, '\n');
  if (stage == 1 && line && strstr(text, "gl_FragCoord")) {
    size_t prefix = (size_t)(line + 1 - text);

    *source = malloc(strlen(text) + strlen(origin) + 1);
    if (*source) {
      memcpy(*source, text, prefix);
      strcpy(*source + prefix, origin);
      strcpy(*source + prefix + strlen(origin), text + prefix);
    }
  } else {
    *source = strdup(text);
  }

  free(adjusted);
  if (*source && (strstr(*source, "packHalf2x16") || strstr(*source, "unpackHalf2x16"))) {
    char *first, *second;
    const char *main;
    const char *helpers = "uint pd_half_bits(float v) {\n"
                          "    uint bits=floatBitsToUint(v), sign=(bits>>16u)&32768u;\n"
                          "    uint magnitude=bits&2147483647u, exponent=magnitude>>23u, mantissa=magnitude&8388607u;\n"
                          "    if (exponent==255u) return sign|31744u|(mantissa==0u?0u:((mantissa>>13u)|512u));\n"
                          "    if (magnitude<947912704u) return sign|uint(roundEven(abs(v)*16777216.0));\n"
                          "    uint rounded=magnitude+4095u+((magnitude>>13u)&1u);\n"
                          "    if (rounded>=1199570944u) return sign|31744u;\n"
                          "    return sign|((rounded-939524096u)>>13u);\n"
                          "}\n"
                          "uint pd_pack_half2(vec2 v) { return pd_half_bits(v.x)|(pd_half_bits(v.y)<<16u); }\n"
                          "float pd_half_float(uint h) {\n"
                          "    uint sign=(h&32768u)<<16u, exponent=(h>>10u)&31u, mantissa=h&1023u;\n"
                          "    if (exponent==0u) return uintBitsToFloat(sign)+float(mantissa)*((sign==0u?1.0:-1.0)*5.9604644775390625e-08);\n"
                          "    return uintBitsToFloat(sign|(exponent==31u?2139095040u:((exponent+112u)<<23u))|(mantissa<<13u));\n"
                          "}\n"
                          "vec2 pd_unpack_half2(uint v) { return vec2(pd_half_float(v&65535u),pd_half_float(v>>16u)); }\n";

    first = replace_word(*source, "unpackHalf2x16", "pd_unpack_half2");
    second = first ? replace_word(first, "packHalf2x16", "pd_pack_half2") : 0;
    free(first);
    free(*source);
    *source = 0;
    if (second) {
      main = strstr(second, "void main()");
      if (main) {
        size_t prefix = main - second;

        *source = malloc(strlen(second) + strlen(helpers) + 1);
        if (*source) {
          memcpy(*source, second, prefix);
          strcpy(*source + prefix, helpers);
          strcpy(*source + prefix + strlen(helpers), main);
        }
      }

      free(second);
    }
  }

  if (!*source) {
    snprintf(error, error_size, "out of memory copying GLSL");
    goto destroy;
  }

  rc = 0;

done:
  if (rc)
    snprintf(error, error_size, "%s", spvc_context_get_last_error_string(context));

destroy:
  spvc_context_destroy(context);
  return rc;
#undef CROSS
}
