/*
 * Copyright 2012, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _CPUIDLE_MODULE_H
#define _CPUIDLE_MODULE_H


#include <module.h>

#include <scheduler.h>


#define CPUIDLE_MODULES_PREFIX	"power/cpuidle"


typedef struct cpuidle_module_info {
	module_info		info;

	float			rank;

	void			(*cpuidle_set_scheduler_mode)(enum scheduler_mode mode);

	void			(*cpuidle_idle)(void);
	void			(*cpuidle_wait)(int32* variable, int32 test);
} cpuidle_module_info;


#endif		// _CPUIDLE_MODULE_H
