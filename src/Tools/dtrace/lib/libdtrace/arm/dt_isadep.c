/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#if defined(__arm__) || defined(__armv6__) || defined(__arm64__)

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

#include <dt_impl.h>
#include <dt_pid.h>

#define SIGNEXTEND(x,v) ((((int) (x)) << (32-v)) >> (32-v))
#define ALIGNADDR(x,v) (((x) >> v) << v)

#define TYPE_SAME 0
#define TYPE_TEXT 1
#define TYPE_DATA 2

#define IS_ARM64_RET(x) (((x) == 0xd65f03c0) || ((x) == 0xd65f0fff)) /* ret, retab */

int
dt_pid_create_entry_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
			  fasttrap_probe_spec_t *ftp, const GElf_Sym *symp)
{
	char dmodel = Pstatus(P)->pr_dmodel;

	ftp->ftps_probe_type = DTFTP_ENTRY;
	ftp->ftps_pc = symp->st_value; // Keep st_value as uint64_t

	if (symp->st_arch_subinfo == 0) {
		dt_dprintf("No arm/thumb information for %s:%s, not instrumenting",ftp->ftps_mod,ftp->ftps_func);
		return (1);
	} else {
		ftp->ftps_arch_subinfo = symp->st_arch_subinfo;
	}

	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_noffs = 1;
	ftp->ftps_offs[0] = 0;

	if (ioctl(dtp->dt_ftfd, FASTTRAPIOC_MAKEPROBE, ftp) != 0) {
		dt_dprintf("fasttrap probe creation ioctl failed: %s",
		    strerror(errno));
		return (dt_set_errno(dtp, errno));
	}

	return (1);
}

/*ARGSUSED*/
static int
dt_pid_create_return_probe32(struct ps_prochandle *P, dtrace_hdl_t *dtp,
			   fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, uint64_t *stret)
{
	/*
	 * This function best read with the ARM architecture reference handy.
	 */

	uint8_t *text, *allocated_text;

	/*
	 * When all the pc relative data is at the end of a function, there are no problems. But
	 * after inlining, it's possible to have constants mixed in with the code. Create a table
	 * to deal with this. It will be the same size as the instructions. When we instrument
	 * instructions, we will have two modes: text and data. For our table, we will have three
	 * values: TYPE_SAME - continue in previous mode; TYPE_TEXT - switch to text; TYPE_DATA -
	 * switch to data.
	 *
	 * When we see a pc-relative load, we assume that is the beginning of a data section. When
	 * we see a branch, we assume that is the beginning of more text.
	 */
	uint8_t *constants;

	ftp->ftps_probe_type = DTFTP_RETURN;
	ftp->ftps_pc = symp->st_value;

	if (symp->st_arch_subinfo == 0) {
		dt_dprintf("No arm/thumb information for %s:%s, not instrumenting",ftp->ftps_mod,ftp->ftps_func);
		return (1);
	} else {
		ftp->ftps_arch_subinfo = symp->st_arch_subinfo;
	}

	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_noffs = 0;

	/*
	 * We allocate a few extra bytes at the end so we don't have to check
	 * for overrunning the buffer.
	 */
	if ((text = allocated_text = calloc(1, symp->st_size + 4)) == NULL) {
		dt_dprintf("mr sparkle: malloc() failed");
		return (DT_PROC_ERR);
	}

	if ((constants = calloc(1, symp->st_size + 4)) == NULL) {
		dt_dprintf("mr sparkle: malloc() failed");
		free(allocated_text);
		return (DT_PROC_ERR);
	}

	/*
	 * We run into all sorts of problems with the ldr instruction offset calculating, if
	 * the original function starts on a halfword. The easiest workaround is to start our
	 * base so that we start on a halfword too. The base address will need to be changed
	 * back before we free the buffer. This problem only occurs in Thumb mode.
	 */
	if ((ftp->ftps_arch_subinfo == 2) && (symp->st_value & 2))
		text = text + 2;

	if (Pread(P, text, symp->st_size, symp->st_value) != symp->st_size) {
		dt_dprintf("mr sparkle: Pread() failed");
		free(allocated_text);
		free(constants);
		return (DT_PROC_ERR);
	}

	/* Approved methods of returning from a function and their valid encodings:
	 * (A = ARM, T = Thumb 16 bit, T2 = Thumb 32 bit)
	 *   ldr pc, [...] (A, T2)
	 *   mov pc, reg (A, T)
	 *   mov pc, immed (A)
	 *   bx reg (A, T)
	 *   b address (A, T, T2)
	 *   ldmfd sp!, [ ... pc ] (A, T2)
	 *   pop { ... pc } (T)
	 *
	 * We also first check for ldr reg, [pc ...] since that is a pc relative load, and a pc relative
	 * load will tell us where any extra data is stored (after the end of a function).
	 */

	if (ftp->ftps_arch_subinfo == 1) {
		/* Arm mode */
		uint32_t* limit = (uint32_t*) (text + ftp->ftps_size);
		uint32_t* inst = (uint32_t*) text;
		ulong_t offset;
		int mode = TYPE_TEXT;

		while (inst < limit) {
			offset = (ulong_t) inst - (ulong_t) text;
			if ((mode == TYPE_DATA && constants[offset] == TYPE_SAME) || constants[offset] == TYPE_DATA) {
				inst++;
				mode = TYPE_DATA;
				continue;
			}
			if (constants[offset] == TYPE_TEXT)
				mode = TYPE_TEXT;

			/*
			 * All of the instructions we instrument are conditional, so if the instruction is
			 * unconditional, skip immediately to the next one.
			 */
			if ((*inst & 0xF0000000) == 0xF0000000) {
				inst++;
				continue;
			}

			if ((*inst & 0x0F7F0000) == 0x051F0000) {
				/* ldr reg, [pc, ...] */
				uint32_t displacement = *inst & 0xFFF;
				ulong_t target = ((uint32_t) inst) + 8;
				if (*inst & (1 << 23)) {
					target += displacement;
				} else {
					target -= displacement;
				}
				if (((ulong_t) inst) < target && target < ((ulong_t) limit))
					constants[(ulong_t) target - (ulong_t) text] = TYPE_DATA;
			}

			if ((*inst & 0x0E50F000) == 0x0410F000) {
				/* ldr pc, [...] */
				/* We are using a jump table to do a jump. This could be equivalent to
				 * either a branch or a branch and link. We only want to instrument
				 * the first case. To identify a link, we will check the previous
				 * instruction to see if it's a mov lr, pc. This is a little fragile,
				 * but it will catch most cases.
				 */
				if (inst > text) {
					/* It's not the first instruction in the function */
					uint32_t prevInst = *(inst-1);

					/* Also check to see if the condition codes are different */
					if ((prevInst & 0xF0000000) != (*inst & 0xF0000000) ||
					    (prevInst & 0x0FFFFFFF) != 0x01A0E00F)
						goto is_ret_arm;
				} else {
					goto is_ret_arm;
				}
			} else if ((*inst & 0x0FFFFFF0) == 0x01A0F000) {
				/* mov pc, reg */
				goto is_ret_arm;
			} else if ((*inst & 0x0FFFF000) == 0x03A0F000) {
				/* mov pc, immed */
				goto is_ret_arm;
			} else if ((*inst & 0x0FFFFFF0) == 0x012FFF10) {
				/* bx reg */
				goto is_ret_arm;
			} else if ((*inst & 0x0F000000) == 0x0A000000) {
				/* branch */
				ulong_t target = ((ulong_t) inst) + 8 + (SIGNEXTEND(*inst & 0x00FFFFFF,24) << 2);
				if (((ulong_t) inst) < target && target < ((ulong_t) limit))
					constants[(ulong_t) target - (ulong_t) text] = TYPE_TEXT;
				if (target < text || limit <= target)
					goto is_ret_arm;
			} else if ((*inst & 0x0FFF8000) == 0x08BD8000) {
				/* ldmfd sp!, { pc ... } */
				goto is_ret_arm;
			}

			inst++;
			continue;

is_ret_arm:
			offset = (ulong_t) inst - (ulong_t) text;
			dt_dprintf("return at offset %lx Arm", offset);
			ftp->ftps_offs[ftp->ftps_noffs++] = offset;
			inst++;
		}
	} else if (ftp->ftps_arch_subinfo == 2) {
		/* Thumb mode */
		uint16_t* limit = (uint16_t*) (text + ftp->ftps_size);
		uint16_t* inst = (uint16_t*) text;
		ulong_t offset;
		int mode = TYPE_TEXT;

		while (inst < limit) {
			offset = (ulong_t) inst - (ulong_t) text;

			if ((mode == TYPE_DATA && constants[offset] == TYPE_SAME) || constants[offset] == TYPE_DATA) {
				/* Data is always one full word */
				inst += 2;
				mode = TYPE_DATA;
				continue;
			}
			if (constants[offset] == TYPE_TEXT)
				mode = TYPE_TEXT;

			if (((*inst >> 11) & 0x1F) > 0x1C) {
				/* Four byte thumb instruction */
				uint16_t* inst2 = inst+1;

				if ((*inst & 0xFF7F) == 0xF85F) {
					/* PC relative load */
					uint32_t displacement = *inst2 & 0xFFF;
					ulong_t target = ALIGNADDR(((uint32_t) inst)+4, 2);
					if (*inst & (1 << 7)) {
						target += displacement;
					} else {
						target -= displacement;
					}
					if (((ulong_t) inst) < target && target < ((ulong_t) limit))
						constants[(ulong_t) target - (ulong_t) text] = TYPE_DATA;
				} else if ((*inst & 0xFF3F) == 0xED1F && (*inst2 & 0x0E00) == 0x0A00) {
					/* PC relative vload */
					uint32_t displacement = (*inst2 * 0xFF) << 2;
					ulong_t target = ALIGNADDR(((uint32_t) inst)+4, 2);
					if (*inst & (1 << 7)) {
						target += displacement;
					} else {
						target -= displacement;
					}
					if (((ulong_t) inst) < target && target < ((ulong_t) limit))
						constants[(ulong_t) target - (ulong_t) text] = TYPE_DATA;
				}

				if ((*inst & 0xF800) == 0xF000 && (*inst2 & 0xD000) == 0x8000) {
					int cond = (*inst >> 6) & 0xF;

					if (cond != 0xE && cond != 0xF) {
						/* Conditional branch */
						int S = (*inst >> 10) & 1, J1 = (*inst2 >> 13) & 1, J2 = (*inst2 >> 11) & 1;
						ulong_t target = ((ulong_t) inst) + 4 + SIGNEXTEND(
							(S << 20) | (J2 << 19) | (J1 << 18) |
							((*inst & 0x003F) << 12) | ((*inst2 & 0x07FF) << 1),
							21);
						if (((ulong_t) inst) < target && target < ((ulong_t) limit))
							constants[(ulong_t) target - (ulong_t) text] = TYPE_TEXT;
						if (target < text || limit <= target)
							goto is_ret_thumb2;
					}
				} else if ((*inst & 0xF800) == 0xF000 && (*inst2 & 0xD000) == 0x9000) {
					/* Unconditional branch */
					int S = (*inst >> 10) & 1, J1 = (*inst2 >> 13) & 1, J2 = (*inst2 >> 11) & 1;
					int I1 = (J1 != S) ? 0 : 1, I2 = (J2 != S) ? 0 : 1;
					ulong_t target = ((ulong_t) inst) + 4 + SIGNEXTEND(
						(S << 24) | (I1 << 23) | (I2 << 22) |
						((*inst & 0x3FF) << 12) | ((*inst2 & 0x7FF) << 1),
						25);
					if (((ulong_t) inst) < target && target < ((ulong_t) limit))
						constants[(ulong_t) target - (ulong_t) text] = TYPE_TEXT;
					if (target < text || limit <= target)
						goto is_ret_thumb2;
				} else if (*inst == 0xE8BD && (*inst2 & 0x8000) == 0x8000) {
					/* ldm sp!, { pc ... } */
					goto is_ret_thumb2;
				} else if ((*inst & 0xFF70) == 0xF850 && (*inst2 & 0xF000) == 0xF000) {
					/* ldr pc, [ ... ] */
					goto is_ret_thumb2;
				}

				inst += 2;
				continue;

is_ret_thumb2:
				offset = (ulong_t) inst - (ulong_t) text;
				dt_dprintf("return at offset %lx Thumb32", offset);
				ftp->ftps_offs[ftp->ftps_noffs++] = offset;
				inst += 2;
			} else {
				/* Two byte thumb instruction */
				if ((*inst & 0xF800) == 0x4800) {
					/* PC relative load */
					ulong_t target = ALIGNADDR(((uint32_t) inst) + ((*inst & 0xFF) * 4) + 4, 2);
					if (((ulong_t) inst) < target && target < ((ulong_t) limit))
						constants[(ulong_t) target - (ulong_t) text] = TYPE_DATA;
				}

				if ((*inst & 0xFF87) == 0x4687) {
					/* mov pc, reg */
					goto is_ret_thumb;
				} else if ((*inst & 0xFF87) == 0x4700) {
					/* bx reg */
					goto is_ret_thumb;
				} else if ((*inst & 0xF000) == 0xD000) {
					int cond = (*inst >> 8) & 0xF;
					if (cond != 0xE && cond != 0xF) {
						/* Conditional branch */
						ulong_t target = ((ulong_t) inst) + 4 + (SIGNEXTEND(*inst & 0xFF,8) << 1);
						if (((ulong_t) inst) < target && target < ((ulong_t) limit))
							constants[(ulong_t) target - (ulong_t) text] = TYPE_TEXT;
						if (target < text || limit <= target)
							goto is_ret_thumb;
					}
				} else if ((*inst & 0xF800) == 0xE000) {
					/* Unconditional branch */
					ulong_t target = ((ulong_t) inst) + 4 + (SIGNEXTEND(*inst & 0x7FF,11) << 1);
					if (((ulong_t) inst) < target && target < ((ulong_t) limit))
						constants[(ulong_t) target - (ulong_t) text] = TYPE_TEXT;
					if (target < text || limit <= target)
						goto is_ret_thumb;
				} else if ((*inst & 0xFF00) == 0xBD00) {
					/* pop { ..., pc} */
					goto is_ret_thumb;
				}

				inst++;
				continue;

is_ret_thumb:
				offset = (ulong_t) inst - (ulong_t) text;
				dt_dprintf("return at offset %lx Thumb16", offset);
				ftp->ftps_offs[ftp->ftps_noffs++] = offset;
				inst++;
			}
		}
	}

	free(allocated_text);
	free(constants);
	if (ftp->ftps_noffs > 0) {
		if (ioctl(dtp->dt_ftfd, FASTTRAPIOC_MAKEPROBE, ftp) != 0) {
			dt_dprintf("fasttrap probe creation ioctl failed: %s",
				strerror(errno));
			return (dt_set_errno(dtp, errno));
		}
	}

	return (ftp->ftps_noffs);
}

/*ARGSUSED*/
static int
dt_pid_create_return_probe64(struct ps_prochandle *P, dtrace_hdl_t *dtp,
			   fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, uint64_t *stret)
{
	uint8_t *text;

	ftp->ftps_probe_type = DTFTP_RETURN;
	ftp->ftps_pc = symp->st_value;

	ftp->ftps_arch_subinfo = symp->st_arch_subinfo;

	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_noffs = 0;

	/*
	 * We allocate a few extra bytes at the end so we don't have to check
	 * for overrunning the buffer.
	 */
	if ((text = calloc(1, symp->st_size + 4)) == NULL) {
		dt_dprintf("mr sparkle: malloc() failed");
		return (DT_PROC_ERR);
	}

	if (Pread(P, text, symp->st_size, symp->st_value) != symp->st_size) {
		dt_dprintf("mr sparkle: Pread() failed");
		free(text);
		return (DT_PROC_ERR);
	}

	uint32_t* limit = (uint32_t*) (text + ftp->ftps_size);
	uint32_t* inst = (uint32_t*) text;

	while (inst < limit) {
		/* b imm26 */
		if (((*inst & 0xfc000000) == 0x14000000)) {
			/*
			 * The offset is from the address of this instruction in the
			 * range +/-128MB, and is encoded as "imm26" times 4.
			 */
			uint32_t target = ((uint32_t) inst) + 4 * SIGNEXTEND(*inst & ((1 << 26) - 1), 26);
			if (target < (uint32_t) text || (uint32_t) limit <= target) {
				goto is_ret;
			}
		}

		/* ret x30 */
		if (IS_ARM64_RET(*inst)) {
			goto is_ret;
		}

		inst++;
		continue;

is_ret:
		dt_dprintf("return at offset %lx Arm64", (ulong_t) inst - (ulong_t) text);
		ftp->ftps_offs[ftp->ftps_noffs++] = (ulong_t) inst - (ulong_t) text;
		inst++;
	}

	free(text);
	if (ftp->ftps_noffs > 0) {
		if (ioctl(dtp->dt_ftfd, FASTTRAPIOC_MAKEPROBE, ftp) != 0) {
			dt_dprintf("fasttrap probe creation ioctl failed: %s",
				strerror(errno));
			return (dt_set_errno(dtp, errno));
		}
	}

	return (ftp->ftps_noffs);
}

/*ARGSUSED*/
int
dt_pid_create_return_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
			   fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, uint64_t *stret)
{
	uint32_t arch = symp->st_arch_subinfo;

	if (arch == FASTTRAP_FN_ARM64 || arch == FASTTRAP_FN_ARM64_32) {
		return dt_pid_create_return_probe64(P, dtp, ftp, symp, stret);
	} else if (arch == FASTTRAP_FN_ARM || arch == FASTTRAP_FN_THUMB) {
		return dt_pid_create_return_probe32(P, dtp, ftp, symp, stret);
	}
	else {
		dt_dprintf("invalid architecture for %s:%s:return: %d", ftp->ftps_mod, ftp->ftps_mod, arch);
		return (1);
	}
}

/*ARGSUSED*/
int
dt_pid_create_offset_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
			   fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, ulong_t off)
{
	/* Not implemented */
	return (1);
}

/*ARGSUSED*/
int
dt_pid_create_glob_offset_probes(struct ps_prochandle *P, dtrace_hdl_t *dtp,
				 fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, const char *pattern)
{
	/* Not implemented */
	return (-1);
}

#endif // __arm__ || __armv6__ || __arm64__
