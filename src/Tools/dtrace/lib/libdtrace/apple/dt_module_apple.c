/*
 *  dt_module_apple.c
 *  dtrace
 *
 *  Created by James McIlree on 3/9/10.
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#if DTRACE_USE_CORESYMBOLICATION
#include <CoreSymbolication/CoreSymbolication.h>
#include <CoreSymbolication/CoreSymbolicationPrivate.h>
#endif /* DTRACE_USE_CORESYMBOLICATION */

#include <libkern/OSAtomic.h>
#include <sys/kas_info.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <dtrace.h>
#include <dt_module.h>
#include <dt_impl.h>
#include <assert.h>

#include <mach-o/nlist.h>

extern int _dtrace_argmax; /* maximum probe arguments */

void
dt_module_get_types(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp,
	dtrace_argdesc_t *adp, int *nargs)
{
	dt_module_t *dmp;
	GElf_Sym sym;
	ctf_funcinfo_t f;
	uint_t id;
	ctf_id_t argv[_dtrace_argmax];
	int argc = sizeof (argv) / sizeof (argv[0]);
	int args = *nargs, i;

	assert(args <= _dtrace_argmax);

	dmp = dt_module_lookup_by_name(dtp, pdp->dtpd_mod);

	/*
	 * Not finding the module is not an error, we'll just carry on
	 * without the argument types
	 */
	if (dmp == NULL)
		goto out;

	if (dmp->dm_ops->do_symname(dmp, pdp->dtpd_func, &sym, &id) == NULL) {
		goto out;
	}
	if (ctf_func_info(dmp->dm_ctfp, id, &f) == CTF_ERR) {
		dt_dprintf("failed to retrieve func info (ctf_errno: %d)", ctf_errno(dmp->dm_ctfp));
		goto out;
	}

	if (strcmp(pdp->dtpd_name, "return") == 0) {
		if (args < 2)
			goto out;
		/*
		 * args[0] on fbt return probes is always the offset of the
		 * returning instruction in the function text
		 */
		bzero(adp, sizeof (dtrace_argdesc_t));
		adp->dtargd_ndx = 0;
		adp->dtargd_id = pdp->dtpd_id;
		adp->dtargd_mapping = adp->dtargd_ndx;
		strncpy(adp->dtargd_native, "int", DTRACE_ARGTYPELEN);
		adp++;

		/*
		 * args[1] contains the return value
		 */
		bzero(adp, sizeof (dtrace_argdesc_t));
		adp->dtargd_ndx = 1;
		adp->dtargd_id = pdp->dtpd_id;
		adp->dtargd_mapping = adp->dtargd_ndx;
		ctf_type_name(dmp->dm_ctfp, f.ctc_return, adp->dtargd_native, DTRACE_ARGTYPELEN);
		*nargs = 2;
	}
	else {
		if (ctf_func_args(dmp->dm_ctfp, id, argc, argv) == CTF_ERR) {
			dt_dprintf("failed to retrieve func args (ctf_errno: %d)", ctf_errno(dmp->dm_ctfp));
			goto out;
		}
		args = MIN(args, f.ctc_argc);
		for (i = 0; i < args; adp++, i++) {
			bzero(adp, sizeof (dtrace_argdesc_t));
			adp->dtargd_ndx = i;
			adp->dtargd_id = pdp->dtpd_id;
			adp->dtargd_mapping = adp->dtargd_ndx;
			ctf_type_name(dmp->dm_ctfp, argv[i], adp->dtargd_native, DTRACE_ARGTYPELEN);
		}
		*nargs = args;
	}
	return;
out:
	/*
	 * We need to indicate that we have no typed arguments if we could not
	 * find the module or an error happened
	 */
	*nargs = 0;
}

#if DTRACE_USE_CORESYMBOLICATION
/*
 * Create a symbolicator by using CoreSymbolication.
 *
 * This function returns a symbolicator.  The symbolicator will be slided with the
 * kernel slide if with_slide is set to true.  For security reason, retrieving the
 * kernel slide requires to be root, otherwise calling this function with with_slide
 * will fail.
 */
static CSSymbolicatorRef
dtrace_kernel_symbolicator(bool with_slide) {
	static pthread_mutex_t symbolicator_lock = PTHREAD_MUTEX_INITIALIZER;
	static CSSymbolicatorRef symbolicator_slide   = { 0, 0 }; // kCSNull isn't considered constant?
	static CSSymbolicatorRef symbolicator_noslide = { 0, 0 }; // kCSNull isn't considered constant?

	assert((!with_slide || geteuid() == 0) && "Retrieving the kernel slide requires to be root");

	CSSymbolicatorRef *const symbolicator_ptr = with_slide ? &symbolicator_slide : &symbolicator_noslide;

	/*
	 * Double checked locking...
	 */
	if (CSIsNull(*symbolicator_ptr)) {
		pthread_mutex_lock(&symbolicator_lock);
		if (CSIsNull(*symbolicator_ptr)) {
			uint32_t flags = kCSSymbolicatorDefaultCreateFlags;
			if (with_slide)
				flags |= kCSSymbolicatorUseSlidKernelAddresses;

			if (_dtrace_disallow_dsym)
				flags |= kCSSymbolicatorDisallowDsymData ;
			CSSymbolicatorRef temp = CSSymbolicatorCreateWithMachKernelFlagsAndNotification(flags, NULL);

			OSMemoryBarrier();
			*symbolicator_ptr = temp;
		}
		pthread_mutex_unlock(&symbolicator_lock);
	}

	return *symbolicator_ptr;
}
#endif /* DTRACE_USE_CORESYMBOLICATION */

static uint8_t
dt_module_nsects(struct load_command *cmd)
{
	if (cmd->cmd == LC_SEGMENT_64) {
		struct segment_command_64 *seg = (struct segment_command_64*) cmd;
		return seg->nsects;
	} else if (cmd->cmd == LC_SEGMENT) {
		struct segment_command *seg = (struct segment_command*) cmd;
		return seg->nsects;
	}
	return 0;
}

static uint64_t
dt_module_cmd_vmaddr(struct load_command *cmd)
{
	if (cmd->cmd == LC_SEGMENT_64) {
		struct segment_command_64 *seg = (struct segment_command_64*) cmd;
		return seg->vmaddr;
	} else if (cmd->cmd == LC_SEGMENT) {
		struct segment_command *seg = (struct segment_command*) cmd;
		return seg->vmaddr;
	}
	return 0;
}

uint64_t dt_module_sym_location(dt_module_t *dmp, uint8_t sect, uint64_t value)
{
#if DTRACE_USE_CORESYMBOLICATION
	static struct load_command *cmd;
	static uint64_t *segs = NULL;
	static uint32_t nsegs = 0;
	struct load_command *cur;
	uint8_t sects = 0;

	static dispatch_once_t once;
	dispatch_once(&once, ^{
		int err;
		size_t size;
		if ((err = kas_info(KAS_INFO_KERNEL_SEGMENT_VMADDR_SELECTOR, NULL, &size)) != 0) {
			dt_dprintf("KAS_INFO_KERNEL_SEGMENT_VMADDR for size failed: %d", errno);
			return;
		}
		nsegs = size / sizeof(uint64_t);
		segs = calloc(nsegs, sizeof(uint64_t));
		if ((err = kas_info(KAS_INFO_KERNEL_SEGMENT_VMADDR_SELECTOR, segs, &size)) != 0) {
			dt_dprintf("KAS_INFO_KERNEL_SEGMENT_VMADDR failed: %d", errno);
			nsegs = 0;
			free(segs);
			segs = NULL;
			return;
		}

		uint32_t *magic = (uint32_t*)elf_getimage(dmp->dm_elf, NULL);
		switch (*magic) {
		case MH_MAGIC:
		case MH_CIGAM: {
			struct mach_header *header = (struct mach_header*)elf_getimage(dmp->dm_elf, NULL);
			nsegs = header->ncmds;
			cmd = (struct load_command*)&header[1];
		}
		break;
		case MH_MAGIC_64:
		case MH_CIGAM_64: {
			struct mach_header_64 *header = (struct mach_header_64*)elf_getimage(dmp->dm_elf, NULL);
			nsegs = header->ncmds;
			cmd = (struct load_command*)&header[1];
		}
		break;
		default:
			dt_dprintf("kernel image has invalid magic: %ux", *magic);
			return;
		}
		/*
		 * Account for LC_UUID / LC_SYMTAB commands in front of the actual segments
		 * in dSYM / static kernelcache symbol files
		 */
		while (cmd->cmd != LC_SEGMENT && cmd->cmd != LC_SEGMENT_64) {
			cmd = (struct load_command *) ((uintptr_t) cmd + cmd->cmdsize);
		}
	});

	if (!segs) {
		return value;
	}

	cur = cmd;
	for (uint8_t i = 0; i < nsegs; i++) {
		sects += dt_module_nsects(cur);
		if (sect < sects) {
			return value - dt_module_cmd_vmaddr(cur) + segs[i];
		}
		cur = (struct load_command *) ((uintptr_t) cur + cur->cmdsize);
	}
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return value;
}


int
dtrace_kernel_path(char *kernel_path, size_t max_length) {
	assert(kernel_path);
#if DTRACE_USE_CORESYMBOLICATION
	char const* path = CSSymbolOwnerGetPath(CSSymbolicatorGetAOutSymbolOwner(dtrace_kernel_symbolicator(false)));
	if (!path) {
		path = CSSymbolOwnerGetPath(CSSymbolicatorGetSymbolOwner(dtrace_kernel_symbolicator(false)));
	}
	if (path) {
		if (strlcpy(kernel_path, path, max_length) < max_length)
			return 0;
	}
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return -1;
}

char*
demangleSymbolCString(const char *mangled)
{
#if DTRACE_USE_CORESYMBOLICATION
	return (char*)CSDemangleSymbolName(mangled);
#else
	return NULL;
#endif /* DTRACE_USE_CORESYMBOLICATION */
}

#if DTRACE_USE_CORESYMBOLICATION
static void filter_module_symbols(CSSymbolOwnerRef owner, CSSymbolIterator valid_symbol)
{
	// See note at callsites, we want to always use __TEXT __text for now.
	if (TRUE || (CSSymbolOwnerIsObject(owner) && !(CSSymbolOwnerGetDataFlags(owner) & kCSSymbolOwnerDataFoundDsym))) {				
		void (^check_sym)(CSSymbolRef) = ^(CSSymbolRef symbol) {
			// By default, the kernel team has requested minimal symbol info.
			if ((CSSymbolIsUnnamed(symbol) == false)) {
				if (CSSymbolGetRange(symbol).length > 0) {
					valid_symbol(symbol);
				}
			}
		};

		// Find the TEXT/TEXT_EXEC text regions
		CSRegionRef text_region = CSSymbolOwnerGetRegionWithName(owner, "__TEXT __text");
		CSRegionRef text_exec_region = CSSymbolOwnerGetRegionWithName(owner, "__TEXT_EXEC __text");
		CSRegionForeachSymbol(text_region, check_sym);
		CSRegionForeachSymbol(text_exec_region, check_sym);

	} else {
		CSSymbolOwnerForeachSymbol(owner, ^(CSSymbolRef symbol) {
			if (CSSymbolIsFunction(symbol) && (CSSymbolGetRange(symbol).length > 0)) {
				valid_symbol(symbol);
			}
		});
	}	
}
#endif /* DTRACE_USE_CORESYMBOLICATION */

/*
 * This method is used to update the kernel's symbol information.
 *
 * The kernel may discard symbol information from kexts and other
 * binaries to save space. This makes lazy initialization difficult
 * for dtrace. As a workaround, the userspace agent querries the
 * kernel during dtrace_open() for a list of missing symbols. This
 * is provided in the form of UUID's for kexts and mach_kernel. Any
 * matching UUID's that can be found in userspace are mined for
 * symbol data, and that data is sent back to the kernel.
 *
 * NOTE... This function is called too early for -xdebug to enable dt_dprintf.
 */
void
dtrace_update_kernel_symbols(dtrace_hdl_t* dtp)
{
#if DTRACE_USE_CORESYMBOLICATION
	uint64_t count = 0;
	
	/* This call is expected to fail with EINVAL */
	dt_ioctl(dtp, DTRACEIOC_MODUUIDSLIST, &count);
	
	if (count) {
		assert(count < 2048);
		dtrace_module_uuids_list_t* uuids_list;
		
		if ((uuids_list = calloc(1, DTRACE_MODULE_UUIDS_LIST_SIZE(count))) == NULL) {
			fprintf(stderr, "Unable to allocate uuids_list for count %llu\n", count);
			return;
		}

		uuids_list->dtmul_count = count;
		if (dt_ioctl(dtp, DTRACEIOC_MODUUIDSLIST, uuids_list) != 0) {
			fprintf(stderr, "Unable to get module uuids list from kernel [%s]\n", strerror(errno));
			goto uuids_cleanup;
		}
		
		CSSymbolicatorRef symbolicator = dtrace_kernel_symbolicator(true);

		if (CSIsNull(symbolicator))
		{
			fprintf(stderr, "Unable to get kernel symbolicator\n");
			goto uuids_cleanup;
		}

		uint32_t i;
		for (i=0; i<uuids_list->dtmul_count; i++) {
			UUID* uuid = &uuids_list->dtmul_uuid[i];

			CFUUIDRef uuid_ref = CFUUIDCreateFromUUIDBytes(NULL, *(CFUUIDBytes*)uuid);
			CSSymbolOwnerRef owner = CSSymbolicatorGetSymbolOwnerWithUUIDAtTime(symbolicator, uuid_ref, kCSNow);

                        //
                        // <rdar://problem/11219724> Please report UUID mismatches when sending symbols to the kernel
                        //
                        if (CSSymbolOwnerGetDataFlags(owner) & kCSSymbolOwnerDataEmpty) {
                                struct stat statinfo; 
                                if (CSSymbolOwnerGetPath(owner) && (stat(CSSymbolOwnerGetPath(owner), &statinfo) == 0)) {
                                        if (S_ISREG(statinfo.st_mode)) {
                                                fprintf(stderr,"WARNING: The file at [%s] does not match the UUID of the version loaded in the kernel\n", CSSymbolOwnerGetPath(owner));
                                        }
                                }
                        }

			// Construct a dtrace_module_symbols_t.
			//
			// First we need the count of symbols. This isn't quite as easy at it would seem at first glance.
			// We have legacy 32b kexts (MH_OBJECT style), 10.7+ 32b kexts (MH_KEXT_BUNDLE), and 64b kexts.
			// The legacy kexts do not properly set their attributes, and so nothing is marked as a function.
			// If we have a legacy kext && it has no dSYM (the dSYM has valid function markers), We instrument
			// everything in the TEXT text section.
			//
			// APPLE NOTE! It turns out there are too many danger dont touch this points that get marked as
			// functions. We're going to bail out to only instrumenting __TEXT __text for everything for now.
			__block uint64_t module_symbols_count = 0;
			filter_module_symbols(owner, ^(CSSymbolRef symbol) { module_symbols_count++; });

			if (module_symbols_count == 0) {
				continue;
			}
			
			// This must be declared before the goto below
			__block uint32_t module_symbol_index = 0;

			//
			// Allocate a correctly sized module symbols
			//
			dtrace_module_symbols_t* module_symbols;
			if ((module_symbols = calloc(1, DTRACE_MODULE_SYMBOLS_SIZE(module_symbols_count))) == NULL) {
				fprintf(stderr, "Unable to allocate module_symbols for count %llu\n", module_symbols_count);
				goto module_symbols_cleanup;
			}
			
			//
			// Fill in the data...
			//
			memcpy(module_symbols->dtmodsyms_uuid, uuid, sizeof(UUID));
			module_symbols->dtmodsyms_count = module_symbols_count;
			filter_module_symbols(owner, ^(CSSymbolRef symbol) {
				dtrace_symbol_t* dtrace_symbol = &module_symbols->dtmodsyms_symbols[module_symbol_index++];
				CSRange range = CSSymbolGetRange(symbol);
				dtrace_symbol->dtsym_addr = range.location;
				dtrace_symbol->dtsym_size = (uint32_t)range.length;
				strlcpy(dtrace_symbol->dtsym_name, CSSymbolGetMangledName(symbol), sizeof(dtrace_symbol->dtsym_name));
			});
			
			//
			// Send it to the kernel!
			//
			if (dt_ioctl(dtp, DTRACEIOC_PROVMODSYMS, module_symbols) != 0) {
				fprintf(stderr, "Unable to send module symbols for %s (count %lld) to kernel [%s]\n", CSSymbolOwnerGetPath(owner), module_symbols->dtmodsyms_count, strerror(errno));
			}

		module_symbols_cleanup:
			if (module_symbols)
				free(module_symbols);
			
			CFRelease(uuid_ref);
		}
		
	uuids_cleanup:
		if (uuids_list)
			free(uuids_list);		
	}
#endif /* DTRACE_USE_CORESYMBOLICATION */
}

/*
 * Exported interface to look up a symbol by address.  We return the GElf_Sym
 * and complete symbol information for the matching symbol.
 */
int dtrace_lookup_by_addr(dtrace_hdl_t *dtp,
                          GElf_Addr addr, 
                          char *aux_sym_name_buffer,	/* auxilary storage buffer for the symbol name */
                          size_t aux_bufsize,		/* size of sym_name_buffer */
                          GElf_Sym *symp,
                          dtrace_syminfo_t *sip)
{
#if DTRACE_USE_CORESYMBOLICATION
	CSSymbolicatorRef kernelSymbolicator = dtrace_kernel_symbolicator(true);

	if (CSIsNull(kernelSymbolicator))
		return (dt_set_errno(dtp, EDT_NOSYMBOLICATOR));
	
        CSSymbolOwnerRef owner = CSSymbolicatorGetSymbolOwnerWithAddressAtTime(kernelSymbolicator, (mach_vm_address_t)addr, kCSNow);

        if (CSIsNull(owner))
                return (dt_set_errno(dtp, EDT_NOSYMADDR));

        if (symp != NULL) {
                CSSymbolOwnerRef symbol = CSSymbolOwnerGetSymbolWithAddress(owner, (mach_vm_address_t)addr);
                if (CSIsNull(symbol))
                        return (dt_set_errno(dtp, EDT_NOSYMADDR));

                CSRange addressRange = CSSymbolGetRange(symbol);

                symp->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
                symp->st_other = 0;
                symp->st_shndx = SHN_MACHO;
                symp->st_value = addressRange.location;
                symp->st_size = addressRange.length;

                if (CSSymbolIsUnnamed(symbol)) {
                        // Hideous awful hack.
                        // Unnamed symbols should display an address.
                        // There is no place to store the addresses.
                        // We force the callers to provide a small auxilary storage buffer...
                        if (aux_sym_name_buffer) {
                                if (CSArchitectureIs64Bit(CSSymbolOwnerGetArchitecture(CSSymbolGetSymbolOwner(symbol))))
                                        snprintf(aux_sym_name_buffer, aux_bufsize, "0x%016llx", CSSymbolGetRange(symbol).location);
                                else
                                        snprintf(aux_sym_name_buffer, aux_bufsize, "0x%08llx", CSSymbolGetRange(symbol).location);                                
                        }

                        symp->st_name = (uintptr_t)aux_sym_name_buffer;
		} else {
                        const char *mangledName;
                        if (_dtrace_mangled &&
                            (mangledName = CSSymbolGetMangledName(symbol)) &&
                            strlen(mangledName) >= 3 &&
                            mangledName[0] == '_' &&
                            mangledName[1] == '_' &&
                            mangledName[2] == 'Z') {
                                // mangled name - use it
                                symp->st_name = (uintptr_t)mangledName;
                        } else {
                                symp->st_name = (uintptr_t)CSSymbolGetName(symbol);
                        }
                }
        }

        if (sip != NULL) {
                sip->dts_object = CSSymbolOwnerGetName(owner);

                if (symp != NULL) {
                        sip->dts_name = (const char *)(uintptr_t)symp->st_name;
                } else {
                        sip->dts_name = NULL;
                }
                sip->dts_id = 0;
        }

        return (0);
#else
	return (dt_set_errno(dtp, EDT_NOSYMBOLICATOR));
#endif /* DTRACE_USE_CORESYMBOLICATION */
}
