/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef SYSINIT_H
#define SYSINIT_H


void init_sysinit();
void uninit_sysinit();
void pd_fbsd_run_sysinit(void **begin, void **end, int reverse);


#endif /* SYSINIT_H */
