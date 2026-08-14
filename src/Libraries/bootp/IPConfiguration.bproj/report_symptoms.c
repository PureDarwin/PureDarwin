/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

/*
 * report_symptoms.c
 * - report symptoms related to address acquisition
 */

/* 
 * Modification History
 *
 * February 23, 2016	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#include <stdint.h>
#include <sys/types.h>
#include <SymptomReporter/SymptomReporter.h>
#include "report_symptoms.h"
#include "symbol_scope.h"
#include <dispatch/dispatch.h>

#define SYMPTOM_REPORTER_configd_NUMERIC_ID	0x68
#define SYMPTOM_REPORTER_configd_TEXT_ID	"com.apple.configd"

#define SYMPTOM_ADDRESS_ACQUISITION_FAILED	0x00068001
#define SYMPTOM_ADDRESS_ACQUISITION_SUCCEEDED	0x00068002

#define INTERFACE_INDEX_QUALIFIER		0

PRIVATE_EXTERN bool
report_address_acquisition_symptom(int ifindex, bool success)
{
    STATIC dispatch_once_t 	S_once;
    STATIC symptom_framework_t	S_reporter;
    bool			reported = false;

    dispatch_once(&S_once, ^{
	    S_reporter
		= symptom_framework_init(SYMPTOM_REPORTER_configd_NUMERIC_ID,
					 SYMPTOM_REPORTER_configd_TEXT_ID);
    });

    if (S_reporter != NULL) {
	symptom_ident_t	ident;
	symptom_t 	symptom;

	ident = success ? SYMPTOM_ADDRESS_ACQUISITION_SUCCEEDED
	    : SYMPTOM_ADDRESS_ACQUISITION_FAILED;
	symptom = symptom_new(S_reporter, ident);
	if (symptom != NULL) {
	    symptom_set_qualifier(symptom, (uint64_t)ifindex,
				  INTERFACE_INDEX_QUALIFIER);
	    reported = (symptom_send(symptom) == 0);
	}
    }
    return (reported);
}
