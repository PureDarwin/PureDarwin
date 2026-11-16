/*
 * Copyright (c) 2006-2008 Apple Computer, Inc.  All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

// This must be done *after* any references to Foundation.h!
#define uint_t  __Solaris_uint_t

#ifdef	__cplusplus
extern "C" {
#endif

#include <dt_impl.h>
#include <dt_provider.h>
#include <dt_string.h>
#include <dt_program.h>
#include "dt_ld.h"

#ifdef	__cplusplus
}
#endif

#include <mach/machine.h>

#include <stdlib.h>
#include <errno.h>
#include <mach/vm_param.h>

#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <memory>

#define dtrace_separator			"$"
#define dtrace_separator_char			'$'

// Why an encoding & decoding prefix? During compilation, the prefix may change...
#define dtrace_stability_encoding_prefix	"___dtrace_stability"
#define dtrace_stability_decoding_prefix	"___dtrace_stability"
#define dtrace_stability_version		"v1"

#define dtrace_typedefs_encoding_prefix	"___dtrace_typedefs"
#define dtrace_typedefs_decoding_prefix	"___dtrace_typedefs"
#define dtrace_typedefs_version		"v2"

#define dtrace_probe_encoding_prefix		"__dtrace_probe"
#define dtrace_probe_decoding_prefix		"___dtrace_probe"
#define dtrace_probe_version			"v1"

#define dtrace_isenabled_encoding_prefix	"__dtrace_isenabled"
#define dtrace_isenabled_decoding_prefix	"___dtrace_isenabled"
#define dtrace_isenabled_version		"v1"

static std::string dt_ld_encode_string(const char* string)
{
        size_t input_length = strlen(string);
        std::shared_ptr<char> results((char*)malloc(input_length * 2 + 1), &::free);
        for (size_t i = 0; i < input_length; i++) {
                sprintf(&results.get()[i*2],"%02x", (unsigned int)string[i]);
        }
        results.get()[input_length*2] = 0;

        return std::string(results.get());
}

static std::string dt_ld_decode_string(const char* string)
{
        size_t input_length = strlen(string) / 2;
        std::shared_ptr<char> results((char*)malloc(input_length * 2 + 1), &::free);
        for (size_t i = 0; i < input_length; i++) {
                unsigned int value;
                sscanf(&string[i*2],"%2x", &value);
                results.get()[i] = (unsigned char)value;
        }
        results.get()[input_length] = 0;

        return std::string(results.get());
}

#pragma mark -
#pragma mark stability encoding / decoding

char* dt_ld_encode_stability(char* provider_name, dt_provider_t *provider)
{
        // Stability info is encoded as (dtrace_stability_encoding_prefix)(providerName)(dtrace_stability_version)(stability_data)
        size_t bufsize = sizeof(dtrace_stability_encoding_prefix) +
        sizeof(dtrace_separator) +
        sizeof(dtrace_stability_version) +
        sizeof(dtrace_separator) +
        strlen(provider_name) +
        sizeof(dtrace_separator) +
        sizeof(dtrace_pattr_t) * 3 + // Each attr is 1 byte * an encoding size of 3 bytes.
        1; // NULL terminator

        char* buffer = (char*)malloc(bufsize);

        snprintf(buffer, bufsize, "%s%s%s%s%s%s%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x_%x",
                 dtrace_stability_encoding_prefix,
                 dtrace_separator,
                 provider_name,
                 dtrace_separator,
                 dtrace_stability_version,
                 dtrace_separator,
                 /* provider attributes */
                 provider->pv_desc.dtvd_attr.dtpa_provider.dtat_name,
                 provider->pv_desc.dtvd_attr.dtpa_provider.dtat_data,
                 provider->pv_desc.dtvd_attr.dtpa_provider.dtat_class,
                 /* module attributes */
                 provider->pv_desc.dtvd_attr.dtpa_mod.dtat_name,
                 provider->pv_desc.dtvd_attr.dtpa_mod.dtat_data,
                 provider->pv_desc.dtvd_attr.dtpa_mod.dtat_class,
                 /* function attributes */
                 provider->pv_desc.dtvd_attr.dtpa_func.dtat_name,
                 provider->pv_desc.dtvd_attr.dtpa_func.dtat_data,
                 provider->pv_desc.dtvd_attr.dtpa_func.dtat_class,
                 /* name attributes */
                 provider->pv_desc.dtvd_attr.dtpa_name.dtat_name,
                 provider->pv_desc.dtvd_attr.dtpa_name.dtat_data,
                 provider->pv_desc.dtvd_attr.dtpa_name.dtat_class,
                 /* args[] attributes */
                 provider->pv_desc.dtvd_attr.dtpa_args.dtat_name,
                 provider->pv_desc.dtvd_attr.dtpa_args.dtat_data,
                 provider->pv_desc.dtvd_attr.dtpa_args.dtat_class);

        return buffer;
}

static const char*
dt_ld_decode_stability_v1_level(int stability_level) {
        switch(stability_level) {
                case DTRACE_STABILITY_INTERNAL: return "INTERNAL";
                case DTRACE_STABILITY_PRIVATE:  return "PRIVATE";
                case DTRACE_STABILITY_OBSOLETE: return "OBSOLETE";
                case DTRACE_STABILITY_EXTERNAL: return "EXTERNAL";
                case DTRACE_STABILITY_UNSTABLE: return "UNSTABLE";
                case DTRACE_STABILITY_EVOLVING: return "EVOLVING";
                case DTRACE_STABILITY_STABLE:   return "STABLE";
                case DTRACE_STABILITY_STANDARD: return "STANDARD";
                default:                        return "ERROR!";
        };
}

static const char*
dt_ld_decode_stability_v1_class(int stability_class) {
        switch(stability_class) {
                case DTRACE_CLASS_UNKNOWN:      return "UNKNOWN";
                case DTRACE_CLASS_CPU:          return "CPU";
                case DTRACE_CLASS_PLATFORM:     return "PLATFORM";
                case DTRACE_CLASS_GROUP:        return "GROUP";
                case DTRACE_CLASS_ISA:          return "ISA";
                case DTRACE_CLASS_COMMON:       return "COMMON";
                default:                        return "ERROR!";
        };
}

static std::string
dt_ld_decode_stability_v1(const char* provider, const char* stability)
{
        // A validly encoded v1 string:
        //
        // ___dtrace_stability$poly$v1$1_1_0_1_1_0_1_1_0_1_1_0_5_5_5
        //
        // The #_#_#_ triples only have single digit legal values.

        if (strlen(stability) == 29) {
                char stability_line_buffer[128];

                const char* name = dt_ld_decode_stability_v1_level(stability[0] - '0');
                const char* data = dt_ld_decode_stability_v1_level(stability[2] - '0');
                const char* stability_class = dt_ld_decode_stability_v1_class(stability[4] - '0');
                snprintf(stability_line_buffer, sizeof(stability_line_buffer), "#pragma D attributes %s/%s/%s provider %s provider\n", name, data, stability_class, provider);
                std::string decoded_stability(stability_line_buffer);
                stability += 6; // Skip to the next encoding

                name = dt_ld_decode_stability_v1_level(stability[0] - '0');
                data = dt_ld_decode_stability_v1_level(stability[2] - '0');
                stability_class = dt_ld_decode_stability_v1_class(stability[4] - '0');
                snprintf(stability_line_buffer, sizeof(stability_line_buffer), "#pragma D attributes %s/%s/%s provider %s module\n", name, data, stability_class, provider);
                decoded_stability += stability_line_buffer;
                stability += 6; // Skip to the next encoding

                name = dt_ld_decode_stability_v1_level(stability[0] - '0');
                data = dt_ld_decode_stability_v1_level(stability[2] - '0');
                stability_class = dt_ld_decode_stability_v1_class(stability[4] - '0');
                snprintf(stability_line_buffer, sizeof(stability_line_buffer), "#pragma D attributes %s/%s/%s provider %s function\n", name, data, stability_class, provider);
                decoded_stability += stability_line_buffer;
                stability += 6; // Skip to the next encoding

                name = dt_ld_decode_stability_v1_level(stability[0] - '0');
                data = dt_ld_decode_stability_v1_level(stability[2] - '0');
                stability_class = dt_ld_decode_stability_v1_class(stability[4] - '0');
                snprintf(stability_line_buffer, sizeof(stability_line_buffer), "#pragma D attributes %s/%s/%s provider %s name\n", name, data, stability_class, provider);
                decoded_stability += stability_line_buffer;
                stability += 6; // Skip to the next encoding

                name = dt_ld_decode_stability_v1_level(stability[0] - '0');
                data = dt_ld_decode_stability_v1_level(stability[2] - '0');
                stability_class = dt_ld_decode_stability_v1_class(stability[4] - '0');
                snprintf(stability_line_buffer, sizeof(stability_line_buffer), "#pragma D attributes %s/%s/%s provider %s args\n", name, data, stability_class, provider);

                return decoded_stability + stability_line_buffer;
        }

        return "/* Error decoding v1 stability string */";
}

static std::vector<std::string> split(const std::string& s, char delim)
{
        std::vector<std::string> elems;
        std::stringstream ss(s);
        std::string item;
        while(std::getline(ss, item, delim)) {
                elems.push_back(item);
        }
        return elems;
}

static std::string dt_ld_decode_stability(std::string encoding)
{
        // A validly encoded v1 string:
        //
        // ___dtrace_stability$poly$v1$1_1_0_1_1_0_1_1_0_1_1_0_5_5_5

        std::vector<std::string> elements = split(encoding, dtrace_separator_char);

        if (elements.size() == 4) {
                if (elements[2] == "v1") {
                        return dt_ld_decode_stability_v1(elements[1].c_str(), elements[3].c_str());
                }
        }

        // Wow, no good way to handle error conditions here.
        return "Unhandled stability encoding version";
}

#pragma mark -
#pragma mark typedef encoding / decoding

// DTrace typedefs a small number of default types by default.
// Unfortunately, these are not base types, and so they are
// encoded as specialized types. This works fine until link
// time, when DTrace sees the encoding as an attempt to redefine
// an existing type, and fails the link. This method creates
// a dictionary of types to ignore when encoding.

typedef std::unordered_set<std::string> ExclusionTypeSet;

static ExclusionTypeSet* base_dtrace_typeset() {
        ExclusionTypeSet* typeset = new ExclusionTypeSet();

        // First walk all 32 bit typedefs
        extern const dt_typedef_t _dtrace_typedefs_32[];
        const dt_typedef_t *iter = _dtrace_typedefs_32;
        for (; iter->dty_src != NULL; iter++) {
                typeset->insert(std::string(iter->dty_dst));
        }

        // These are almost certainly the same, but just in case...
        // Walk all 64 bit typedefs
        extern const dt_typedef_t _dtrace_typedefs_64[];
        iter = _dtrace_typedefs_64;
        for (; iter->dty_src != NULL; iter++) {
                typeset->insert(std::string(iter->dty_dst));
        }

        return typeset;
}

static bool is_base_dtrace_type(std::string type) {
        static ExclusionTypeSet* exclusion_types = base_dtrace_typeset();
        return exclusion_types->find(type) != exclusion_types->end();
}

//
// If the input type is a pointer, return the type pointed to.
// This method is recursive, it will walk back a chain of pointers
// until it reaches a non pointer type.
//
// If the original type is not a pointer, it is returned unchanged.

static ctf_id_t dt_ld_strip_pointers(ctf_file_t *file, ctf_id_t type) {
        if (ctf_type_kind(file, type) == CTF_K_POINTER) {
                return dt_ld_strip_pointers(file, ctf_type_reference(file, type));
        }

        return type;
}

// This method requires the caller have a valid NSAutoreleasePool
//
// This method works as follows:
//
// 1) Strip any pointer'dness from the arg type
// 2) If the resulting type != a base type, assume it needs a typedef
// 3) We *DO NOT* retain the original type. Everything that is typedef'd is forced to int, I.E.
//
//    original:   typedef float*** foo_t
//    encoded:    typedef int foo_t

typedef std::unordered_map< std::string, std::string > TypeEncodingMap;

static int dt_ld_probe_encode_typedef_iter(dt_idhash_t *dhp, dt_ident_t *idp, void *data)
{
#pragma unused(dhp)
        TypeEncodingMap* encoded_types_map = (TypeEncodingMap*)data;
        dt_probe_t* probe = (dt_probe_t*)idp->di_data;
        dt_node_t* node;
        for (node = probe->pr_nargs; node != NULL; node = node->dn_list) {
                ctf_id_t stripped_of_pointers = dt_ld_strip_pointers(node->dn_ctfp, node->dn_type);
                ctf_id_t base = ctf_type_resolve(node->dn_ctfp, stripped_of_pointers);
                if (base != stripped_of_pointers) {
                        ssize_t size = ctf_type_lname(node->dn_ctfp, stripped_of_pointers, NULL, 0) + 1;
                        char* buf = (char*)alloca(size);
                        ctf_type_lname(node->dn_ctfp, stripped_of_pointers, buf, size);
                        std::string type_key(buf);

                        // Gah. DTrace always typedefs a certain set of types, which are not base types.
                        // See <rdar://problem/5194316>. I haven't been able to discover a way to differentiate
                        // the predefined types from those created in provider.d files, so we do this the hard
                        // way.

                        if (!is_base_dtrace_type(type_key)) {
                                if (encoded_types_map->find(type_key) == encoded_types_map->end()) {
                                        encoded_types_map->insert(TypeEncodingMap::value_type(type_key, dt_ld_encode_string(type_key.c_str())));
                                }
                        }
                }
        }

        return 0;
}

char* dt_ld_encode_typedefs(char* provider_name, dt_provider_t *provider)
{
        TypeEncodingMap encoded_types_map;

        dt_idhash_iter(provider->pv_probes, dt_ld_probe_encode_typedef_iter, (void*)&encoded_types_map);

        char encoded_typedefs_line_buffer[256];
        snprintf(encoded_typedefs_line_buffer, sizeof(encoded_typedefs_line_buffer), "%s%s%s%s%s",
                 dtrace_typedefs_encoding_prefix,
                 dtrace_separator,
                 provider_name,
                 dtrace_separator,
                 dtrace_typedefs_version);
        std::string encoded_types(encoded_typedefs_line_buffer);

        for (TypeEncodingMap::iterator it = encoded_types_map.begin(); it != encoded_types_map.end(); ++it) {
                encoded_types += dtrace_separator;
                encoded_types += it->second;
        }

        if (_dtrace_debug) {
                for (TypeEncodingMap::iterator it = encoded_types_map.begin(); it != encoded_types_map.end(); ++it) {
                        dt_dprintf("dt_ld encoding type %s as %s", it->first.c_str(), it->second.c_str());
                }
        }

        return strdup(encoded_types.c_str());
}

static std::string dt_ld_decode_typedefs_v1(std::vector<std::string>& typedefs)
{
        std::string decoded;

        for (size_t i = 3; i < typedefs.size(); i++) {
                char line_buffer[128];
                snprintf(line_buffer, sizeof(line_buffer), "typedef int %s;\n", dt_ld_decode_string(typedefs[i].c_str()).c_str());
                decoded += line_buffer;
        }

        return decoded;
}

static std::string dt_ld_decode_typedefs(std::string encoding, std::string* version_out)
{
        std::vector<std::string> elements = split(encoding, dtrace_separator_char);

        std::string version;
        if (elements.size() >= 3) {
                version = elements[2];
                if (version_out) {
                        *version_out = elements[2];
                }
        }

        // Is anything actually encoded?
        if (elements.size() >= 4) {
                // Both v1 & v2 use the same format, v2 is a subset of v1 (with the fix for <rdar://problem/5194316>)
                if (version == "v1" || version == "v2") {
                        return dt_ld_decode_typedefs_v1(elements);
                }

                return "Unhandled typedefs encoding version";
        }

        return "";
}

#pragma mark -
#pragma mark probe encoding / decoding

char* dt_ld_encode_probe(char* provider_name, char* probe_name, dt_probe_t* probe)
{
        char line_buffer[256];
        snprintf(line_buffer, sizeof(line_buffer), "%s%s%s%s%s%s%s",
                 dtrace_probe_encoding_prefix,
                 dtrace_separator,
                 provider_name,
                 dtrace_separator,
                 probe_name,
                 dtrace_separator,
                 dtrace_probe_version);

        std::string encoded(line_buffer);

        for (int i = 0; i < probe->pr_nargc; i++) {
                dt_node_t* node = probe->pr_nargv[i];
                ssize_t size = ctf_type_lname(node->dn_ctfp, node->dn_type, NULL, 0) + 1;
                char* buf = (char*)alloca(size);
                ctf_type_lname(node->dn_ctfp, node->dn_type, buf, size);
                encoded += dtrace_separator_char;
                encoded += dt_ld_encode_string(buf);
        }

        return strdup(encoded.c_str());
}

static std::string dt_ld_decode_probe_v1(std::vector<std::string>& arguments)
{
        std::string decoded;

        for (size_t i = 4; i < arguments.size(); ++i) {
                decoded += dt_ld_decode_string(arguments[i].c_str());
                if (i+1 < arguments.size()) {
                        decoded += ',';
                }
        }

        return decoded;
}

static std::string dt_ld_decode_probe(std::string encoding)
{
        std::vector<std::string> elements = split(encoding, dtrace_separator_char);

        if (elements.size() > 3) {
                if (elements[3] == "v1") {
                        std::string probe("\tprobe ");
                        probe += elements[2];
                        probe += '(';

                        if (elements.size() > 4) {
                                probe += dt_ld_decode_probe_v1(elements);
                        }

                        probe += ");";

                        return probe;
                }
        }

        // Wow, no good way to handle error conditions here.
        return "Unhandled probe encoding version";
}

#pragma mark -
#pragma mark isenabled encoding

char* dt_ld_encode_isenabled(char* provider_name, char* probe_name)
{
        // "isenabled" probe info is encoded as (dtrace_isenabled_encoding_prefix)(providerName)(probe_name)
        size_t bufsize = sizeof(dtrace_isenabled_encoding_prefix) +
                                                sizeof(dtrace_separator) +
                                                strlen(provider_name) +
                                                sizeof(dtrace_separator) +
                                                strlen(probe_name) +
                                                1; // NULL terminator

        char* buffer = (char*)malloc(bufsize);

        snprintf(buffer, bufsize, "%s%s%s%s%s%s%s",
                 dtrace_isenabled_encoding_prefix,
                 dtrace_separator,
                 provider_name,
                 dtrace_separator,
                 probe_name,
                 dtrace_separator,
                 dtrace_isenabled_version);

        return buffer;
}

#pragma mark -
#pragma mark D Script regeneration

static std::string dt_ld_decode_script(std::string stability, std::string typedefs, std::vector<std::string>& probes)
{
        std::string decoded_typedefs = dt_ld_decode_typedefs(typedefs, NULL);

        std::string script = dt_ld_decode_typedefs(typedefs, NULL);

        script += "\nprovider ";
        // Maybe we should pass the provider name in? Do some error checking?
        script += split(stability, dtrace_separator_char)[1]; // provider name
        script += " {\n";

        std::unordered_set<std::string> uniqued_probes;
        for (std::vector<std::string>::iterator it = probes.begin(); it < probes.end(); ++it) {
                std::vector<std::string> components = split(*it, dtrace_separator_char);

                if (components.size() < 3) // Can't be a probe spec
                        continue;

                if (components[0] != dtrace_probe_decoding_prefix)
                        continue;

                std::string probe_name = components[2];

                if (uniqued_probes.count(probe_name))
                        continue;

                uniqued_probes.insert(probe_name);
                script += dt_ld_decode_probe(*it);
                script += '\n';
        }

        script += "};\n\n";
        script += dt_ld_decode_stability(stability);
        script += '\n';

        return script;
}

#pragma mark -
#pragma mark Linker support

static int linker_flags(cpu_type_t cpu)
{
        int oflags = 0;

        oflags |= DTRACE_O_NODEV;

        if(cpu & CPU_ARCH_ABI64)
                oflags |= DTRACE_O_LP64;
        else
                oflags |= DTRACE_O_ILP32;

        return oflags;
}

static void set_options(dtrace_hdl_t* dtp)
{
        (void) dtrace_setopt(dtp, "linkmode", "dynamic");
        (void) dtrace_setopt(dtp, "unodefs", NULL);
        (void) dtrace_setopt(dtp, "nolibs", NULL); /* In case /usr/lib/dtrace/ * is broken, we can succeed. */
}

static int register_probes(dtrace_hdl_t* dtp, int count, const char* labels[], const char* functions[])
{
        int i;
        int is_enabled = 0;

        for(i = 0; i < count; i++) {
                const char* label = labels[i];
                const char* label0 = label;

                if(strncmp(label, dtrace_isenabled_decoding_prefix, sizeof(dtrace_isenabled_decoding_prefix) - 1) == 0) {
                        // skip prefix
                        label += sizeof(dtrace_isenabled_decoding_prefix) - 1;
                        is_enabled = 1;
                } else if (strncmp(label, dtrace_probe_decoding_prefix, sizeof(dtrace_probe_decoding_prefix) - 1) == 0) {
                        // skip prefix
                        label += sizeof(dtrace_probe_decoding_prefix) - 1;
                        is_enabled = 0;
                } else {
                        fprintf(stderr, "error: invalid probe marker: %s\n", label0);
                        return -1;
                }

                // skip separator

                label += sizeof(dtrace_separator) - 1;

                // Grab the provider name

                const char* end = strstr(label, dtrace_separator);
                if(!end) {
                        fprintf(stderr, "error: probe marker contains no provider name: %s\n", label0);
                        return -1;
                }

                char* provider_name = (char*)malloc(end - label + 1);
                memcpy(provider_name, label, (end - label));
                provider_name[end - label] = 0;

                // Skip the separator

                label = end + sizeof(dtrace_separator) - 1;

                // Grab the probe name

                end = strstr(label, dtrace_separator);

                if(!end) {
                        fprintf(stderr, "error: probe marker contains no probe name: %s\n", label0);
                        return -1;
                }

                char* probe_name = (char*)malloc(end - label + 1);
                memcpy(probe_name, label, (end - label));
                probe_name[end - label] = 0;
                probe_name = strhyphenate(probe_name);

                // now, register the probe
                dt_provider_t *provider = dt_provider_lookup(dtp, provider_name);
                if(!provider) {
                        fprintf(stderr, "error: provider %s doesn't exist\n", provider_name);
                        return -1;
                }

                dt_probe_t* probe = dt_probe_lookup(provider, probe_name);
                if(!probe) {
                        fprintf(stderr, "error: probe %s doesn't exist\n", probe_name);
                        return -1;
                }

                // The "raw" function names provided by the linker will have an underscore
                // prepended. Remove it before registering the probe function.
                const char* function_name = functions[i];
                if (function_name[0] == '_')
                        function_name++;

                if(dt_probe_define(provider, probe, function_name, NULL, i, is_enabled)) {
                        fprintf(stderr, "error: couldn't define probe %s:::%s\n", provider_name, probe_name);
                        return -1;
                }

                free(provider_name);            // free() of provider_name
                free(probe_name);               // free() of probe_name
        }

        return 0;
}

static int register_offsets(dof_hdr_t* header, int count, uint64_t offsetsInDOF[])
{
        dof_sec_t* sections = (dof_sec_t*)((char*)header + header->dofh_secoff);

        int i;

        for(i = 0; i < count; i++)
                offsetsInDOF[i] = (uint64_t)-1;

        for(i = 0; i < header->dofh_secnum; i++) {
                switch(sections[i].dofs_type) {
                case DOF_SECT_PROFFS:
                case DOF_SECT_PRENOFFS:
                        {
                                // As each probe is defined, it gets a uint32_t entry that indicates its offset.
                                // In the Sun DOF, this is the offset from the start of the function the probe
                                // resides in. We use it to mean "offset to this probe from the start of the
                                // DOF section". We stored the relocation_index as a placeholder in the
                                // register_probes() function.
                                uint32_t* probe_offsets = (uint32_t*)((char*)header + sections[i].dofs_offset);
                                uint32_t j, sec_count = sections[i].dofs_size / sizeof(uint32_t);
                                for (j=0; j<sec_count; j++) {
                                        int relocation_index = probe_offsets[j];
                                        offsetsInDOF[relocation_index] = (uint64_t)(unsigned long)((char*)&probe_offsets[j] - (char*)header);
                                }
                        }
                        break;
                }
        }

        return 0;
}

void* dtrace_ld_create_dof(cpu_type_t cpu,             // [provided by linker] target architecture
                           unsigned int typeCount,     // [provided by linker] number of stability or typedef symbol names
                           const char* typeNames[],    // [provided by linker] stability or typedef symbol names
                           unsigned int probeCount,    // [provided by linker] number of probe or isenabled locations
                           const char* probeNames[],   // [provided by linker] probe or isenabled symbol names
                           const char* probeWithin[],  // [provided by linker] function name containing probe or isenabled
                           uint64_t offsetsInDOF[],    // [allocated by linker, populated by DTrace] per-probe offset in the DOF
                           size_t* size)               // [allocated by linker, populated by DTrace] size of the DOF)
{
        bool printReconstructedScript = getenv("DTRACE_PRINT_RECONSTRUCTED_SCRIPT") != NULL;

        int i, err;
        const char* stability = NULL;
        const char* typedefs = NULL;

        // First, find a valid stability and typedefs.
        for (i=0; i<typeCount; i++) {
                if (strncmp(typeNames[i], dtrace_stability_decoding_prefix, sizeof(dtrace_stability_decoding_prefix)-1) == 0) {
                        if (stability == NULL) {
                                stability = typeNames[i];
                        } else if (strcmp(stability, typeNames[i]) != 0) {
                                fprintf(stderr, "error: Found conflicting dtrace stability info:\n%s\n%s\n", stability, typeNames[i]);
                                return NULL;
                        }
                } else if (strncmp(typeNames[i], dtrace_typedefs_decoding_prefix, sizeof(dtrace_typedefs_decoding_prefix)-1) == 0) {
                        if (typedefs == NULL) {
                                typedefs = typeNames[i];
                        } else if (strcmp(typedefs, typeNames[i]) != 0) {
                                // let's see if it's from a version conflict.
                                std::string existing_info;
                                std::string new_info;
                                dt_ld_decode_typedefs(typedefs, &existing_info);
                                dt_ld_decode_typedefs(typeNames[i], &new_info);
                                if (existing_info != new_info) {
                                        fprintf(stderr,
                                                "error: Found dtrace typedefs generated by "
                                                "different versions of dtrace:\n%s (%s)\n%s (%s)\n",
                                                typedefs, existing_info.c_str(),
                                                typeNames[i], new_info.c_str());
                                        fprintf(stderr, "Please try regenerating all dtrace created "
                                                "header files with the same version of "
                                                "dtrace before rebuilding your project.\n");
                                } else {
                                        fprintf(stderr, "error: Found conflicting dtrace typedefs info:\n%s\n%s\n", typedefs, typeNames[i]);
                                }
                                return NULL;
                        }
                } else {
                        fprintf(stderr, "error: Found unhandled dtrace typename prefix: %s\n", typeNames[i]);
                        return NULL;
                }
        }

        if (stability == NULL) {
                fprintf(stderr, "error: Must have a valid dtrace stability entry\n");
                return NULL;
        }

        if (typedefs == NULL) {
                fprintf(stderr, "error: Must have a a valid dtrace typedefs entry\n");
                return NULL;
        }

        // Recreate the provider.d script
        std::vector<std::string> probes;
        int is_enabled_probes = 0;
        for (i=0; i<probeCount; i++) {
                if (strncmp(probeNames[i], dtrace_probe_decoding_prefix, sizeof(dtrace_probe_decoding_prefix)-1) == 0) {
                        // Assert this belongs to the correct provider!
                        probes.push_back(std::string(probeNames[i]));
                } else if (strncmp(probeNames[i], dtrace_isenabled_decoding_prefix, sizeof(dtrace_isenabled_decoding_prefix)-1) == 0) {
                        // Assert this belongs to the correct provider!
                        probes.push_back(std::string(probeNames[i]));
                        is_enabled_probes++;
                } else {
                        fprintf(stderr, "error: Found unhandled dtrace probe prefix: %s\n", probeNames[i]);
                }
        }

        if (probes.size() == 0) {
                fprintf(stderr, "error: Dtrace provider %s has no probes\n", stability);
                return NULL;
        }

        std::string dscript = dt_ld_decode_script(std::string(stability), std::string(typedefs), probes);

        dtrace_hdl_t* dtp = dtrace_open(DTRACE_VERSION,
                                        linker_flags(cpu),
                                        &err);

        if(!dtp) {
                fprintf(stderr,"error: Failed to initialize dtrace: %s\n", dtrace_errmsg(NULL, err));
                return NULL;
        }

        set_options(dtp);

        dtrace_prog_t* program = dtrace_program_strcompile(dtp,
                                                           dscript.c_str(),
                                                           DTRACE_PROBESPEC_NONE,
                                                           0,
                                                           0,
                                                           NULL);

        if(!program) {
                fprintf(stderr, "error: Could not compile reconstructed dtrace script:\n\n%s\n", dscript.c_str());
                return NULL;
        }

        if (_dtrace_debug || printReconstructedScript) {
                fprintf(stderr, "\n%s\n", dscript.c_str());
        }

        if(register_probes(dtp, probeCount, probeNames, probeWithin)) {
                fprintf(stderr, "error: Could not register probes\n");
                return NULL;
        }

        dof_hdr_t* dof = (dof_hdr_t*)dtrace_dof_create(dtp, program, DTRACE_D_PROBES | DTRACE_D_STRIP);

        if(register_offsets(dof, probeCount, offsetsInDOF)) {
                fprintf(stderr, "error: Could not register DOF offsets\n");
                return NULL;
        }

        *size = dof->dofh_filesz;

        void* return_data = malloc(*size);
        memcpy(return_data, dof, *size);

        dtrace_dof_destroy(dtp, dof);
        dtrace_close(dtp);

        return return_data;
}
