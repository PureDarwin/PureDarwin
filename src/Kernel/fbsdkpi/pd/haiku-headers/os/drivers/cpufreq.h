/*
 * Copyright 2013, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _CPUFREQ_H
#define _CPUFREQ_H


#include <module.h>

#include <scheduler.h>


#define CPUFREQ_MODULES_PREFIX	"power/cpufreq"


const int kCPUPerformanceScaleMax = 1000;

typedef struct cpufreq_module_info {
	module_info		info;

	float			rank;

	void			(*cpufreq_set_scheduler_mode)(enum scheduler_mode mode);

	status_t		(*cpufreq_increase_performance)(int delta);
	status_t		(*cpufreq_decrease_performance)(int delta);
} cpufreq_module_info;


#endif	// _CPUFREQ_H

