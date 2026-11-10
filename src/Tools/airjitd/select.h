// select.h - which executor runs draws and dispatches: Vulkan, OpenGL or the CPU JIT
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef AIRJITD_SELECT_H
#define AIRJITD_SELECT_H

enum pd_backend_kind { PD_BACKEND_NONE, PD_BACKEND_VULKAN, PD_BACKEND_OPENGL, PD_BACKEND_CPU };

// AIRJITD_BACKEND=vk|gl|cpu, or the older AIRJITD_VULKAN=1 and AIRJITD_OPENGL=1, PD_BACKEND_NONE when unset
enum pd_backend_kind pd_backend_forced(void);

// chosen once: the forced one when it comes up, else a Vulkan GPU, then hardware GL, then the CPU.
// jobs the chosen GPU executor declines still run on the CPU
enum pd_backend_kind pd_backend_choose(void);

const char *pd_backend_name(enum pd_backend_kind k);

#endif
