/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <os/collections.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <darwintest.h>


T_DECL(map_basic_64, "Make sure 64 bit map basics work",
		T_META("owner", "Core Darwin Daemons & Tools"))
{
	os_map_64_t basic_64_map;
	__block bool got_cafebabe = false;
	__block bool got_deadbeaf = false;
	uint64_t value;

	T_LOG("Start");

	// *** BASIC 64 bit key testing ***

	os_map_init(&basic_64_map, NULL);

	T_ASSERT_EQ(os_map_count(&basic_64_map), 0, "Expect map to be empty");

	os_map_insert(&basic_64_map, 0xCAFECAFE, (void *)0xCAFEBABE);
	os_map_insert(&basic_64_map, 0xDEADDEAD, (void *)0xDEADBEEF);

	T_ASSERT_EQ(os_map_count(&basic_64_map), 2,
		    "Expect map to have 2 entries");

	os_map_foreach(&basic_64_map, ^bool (uint64_t key, void *value){
		T_LOG("Foreach called for 0x%llx, 0x%llx",
		      (unsigned long long)key, (unsigned long long)value);
		if (key == 0xCAFECAFE) {
			T_ASSERT_EQ(value, (void *)0xCAFEBABE,
				    "Callback expect 0xCAFEBABE");
			got_cafebabe = true;
		} else if (key == 0xDEADDEAD) {
			T_ASSERT_EQ(value, (void *)0xDEADBEEF,
				    "Callback expec 0xDEADBEEF");
			got_deadbeaf = true;
		} else {
			T_FAIL("Got unexpected callback 0x%llx, 0x%llx",
			       (unsigned long long)key,
			       (unsigned long long)value);
		}
		return true;
	});

	if (!got_cafebabe || !got_deadbeaf) {
		T_FAIL("Failed to get callback");
	}

	value = (uint64_t)os_map_find(&basic_64_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0xDEADBEEF, "Find 1");

	value = (uint64_t)os_map_find(&basic_64_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "Find 2");

	value = (uint64_t)os_map_find(&basic_64_map, 0xFF00F0F0);
	T_ASSERT_EQ(value, (uint64_t)0x0, "Find 3");


	os_map_delete(&basic_64_map, 0xDEADDEAD);

	T_ASSERT_EQ(os_map_count(&basic_64_map), 1,
		    "Expect map to have 1 entries");

	value = (uint64_t)os_map_find(&basic_64_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 1");

	value = (uint64_t)os_map_find(&basic_64_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "After-delete find 2");

	os_map_delete(&basic_64_map, 0xCAFECAFE);

	T_ASSERT_EQ(os_map_count(&basic_64_map), 0,
		    "Expect map to be empty");

	value = (uint64_t)os_map_find(&basic_64_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 3");

	value = (uint64_t)os_map_find(&basic_64_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete find 4");

	os_map_destroy(&basic_64_map);
}

T_DECL(map_basic_32, "Make sure 32 bit map basics work",
		T_META("owner", "Core Darwin Daemons & Tools"))
{
	os_map_32_t basic_32_map;
	__block bool got_cafebabe = false;
	__block bool got_deadbeaf = false;
	uint64_t value;

	T_LOG("Start");

	os_map_init(&basic_32_map, NULL);

	T_ASSERT_EQ(os_map_count(&basic_32_map), 0, "Expect map to be empty");

	os_map_insert(&basic_32_map, 0xCAFECAFE, (void *)0xCAFEBABE);
	os_map_insert(&basic_32_map, 0xDEADDEAD, (void *)0xDEADBEEF);

	T_ASSERT_EQ(os_map_count(&basic_32_map), 2,
		    "Expect map to have 2 entries");

	os_map_foreach(&basic_32_map, ^bool (uint32_t key, void *value){
		T_LOG("Foreach called for 0x%llx, 0x%llx",
		      (unsigned long long)key, (unsigned long long)value);
		if (key == 0xCAFECAFE) {
			T_ASSERT_EQ(value, (void *)0xCAFEBABE,
				    "Callback expect 0xCAFEBABE");
			got_cafebabe = true;
		} else if (key == 0xDEADDEAD) {
			T_ASSERT_EQ(value, (void *)0xDEADBEEF,
				    "Callback expec 0xDEADBEEF");
			got_deadbeaf = true;
		} else {
			T_FAIL("Got unexpected callback 0x%llx, 0x%llx",
			       (unsigned long long)key,
			       (unsigned long long)value);
		}
		return true;
	});

	if (!got_cafebabe || !got_deadbeaf) {
		T_FAIL("Failed to get callback");
	}

	value = (uint64_t)os_map_find(&basic_32_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0xDEADBEEF, "Find 1");

	value = (uint64_t)os_map_find(&basic_32_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "Find 2");

	value = (uint64_t)os_map_find(&basic_32_map, 0xFF00F0F0);
	T_ASSERT_EQ(value, (uint64_t)0x0, "Find 3");

	os_map_delete(&basic_32_map, 0xDEADDEAD);

	T_ASSERT_EQ(os_map_count(&basic_32_map), 1,
		    "Expect map to have 1 entries");

	value = (uint64_t)os_map_find(&basic_32_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 1");

	value = (uint64_t)os_map_find(&basic_32_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "After-delete find 2");

	os_map_delete(&basic_32_map, 0xCAFECAFE);

	T_ASSERT_EQ(os_map_count(&basic_32_map), 0, "Expect map to be empty");

	value = (uint64_t)os_map_find(&basic_32_map, 0xDEADDEAD);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 3");

	value = (uint64_t)os_map_find(&basic_32_map, 0xCAFECAFE);
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete find 4");

	os_map_destroy(&basic_32_map);
}


T_DECL(map_basic_string, "Make sure string map basics work",
		T_META("owner", "Core Darwin Daemons & Tools"))
{
	os_map_str_t basic_string_map;
	__block bool got_cafebabe = false;
	__block bool got_deadbeaf = false;
	uint64_t value;

	T_LOG("Start");

	os_map_init(&basic_string_map, NULL);

	T_ASSERT_EQ(os_map_count(&basic_string_map), 0,
		    "Expect map to be empty");

	os_map_insert(&basic_string_map, "0xCAFECAFE", (void *)0xCAFEBABE);
	os_map_insert(&basic_string_map, "0xDEADDEAD", (void *)0xDEADBEEF);

	T_ASSERT_EQ(os_map_count(&basic_string_map), 2,
		    "Expect map to have 2 entries");

	os_map_foreach(&basic_string_map, ^bool (const char *key, void *value){
		T_LOG("Foreach called for 0x%llx, 0x%llx",
		      (unsigned long long)key, (unsigned long long)value);
		if (strcmp("0xCAFECAFE", key) == 0) {
			T_ASSERT_EQ(value, (void *)0xCAFEBABE,
				    "Callback expect 0xCAFEBABE");
			got_cafebabe = true;
		} else if (strcmp("0xDEADDEAD", key) == 0) {
			T_ASSERT_EQ(value, (void *)0xDEADBEEF,
				    "Callback expec 0xDEADBEEF");
			got_deadbeaf = true;
		} else {
			T_FAIL("Got unexpected callback 0x%llx, 0x%llx",
			       (unsigned long long)key,
			       (unsigned long long)value);
		}
		return true;
	});

	if (!got_cafebabe || !got_deadbeaf) {
		T_FAIL("Failed to get callback");
	}

	value = (uint64_t)os_map_find(&basic_string_map, "0xDEADDEAD");
	T_ASSERT_EQ(value, (uint64_t)0xDEADBEEF, "Find 1");

	value = (uint64_t)os_map_find(&basic_string_map, "0xCAFECAFE");
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "Find 2");

	value = (uint64_t)os_map_find(&basic_string_map, "0xFF00F0F0");
	T_ASSERT_EQ(value, (uint64_t)0x0, "Find 3");


	os_map_delete(&basic_string_map, "0xDEADDEAD");

	T_ASSERT_EQ(os_map_count(&basic_string_map), 1,
		    "Expect map to have 1 entries");

	value = (uint64_t)os_map_find(&basic_string_map, "0xDEADDEAD");
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 1");

	value = (uint64_t)os_map_find(&basic_string_map, "0xCAFECAFE");
	T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "After-delete find 2");

	os_map_delete(&basic_string_map, "0xCAFECAFE");

	T_ASSERT_EQ(os_map_count(&basic_string_map), 0,
		    "Expect map to be empty");

	value = (uint64_t)os_map_find(&basic_string_map, "0xDEADDEAD");
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 3");

	value = (uint64_t)os_map_find(&basic_string_map, "0xCAFECAFE");
	T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete find 4");

	os_map_destroy(&basic_string_map);
}

T_DECL(map_entry_string, "Make sure string entry fetching works",
		T_META("owner", "Core Darwin Daemons & Tools"))
{

	os_map_str_t basic_string_map;

	T_LOG("Start");

	os_map_init(&basic_string_map, NULL);

	os_map_insert(&basic_string_map, "CAFE", (void *)0xCAFEBABE);

	// Extra steps are taken to make sure the lookup strings aren't compiled
	// to the same pointer.
	volatile char lookup_string_1[5];
	sprintf(lookup_string_1, "CAFE");
	volatile char lookup_string_2[5];
	sprintf(lookup_string_2, "CAFE");

	T_ASSERT_EQ(strcmp(&lookup_string_1, "CAFE"), 0,
		    "Expect lookup strings to be CAFE");
	T_ASSERT_EQ(strcmp(&lookup_string_2, "CAFE"), 0,
		    "Expect lookup strings to be CAFE");
	T_ASSERT_NE(&lookup_string_1, &lookup_string_2,
		    "Expect lookup strings to be different");

	const char *entry_string_1 = os_map_entry(&basic_string_map,
						  &lookup_string_1);

	T_ASSERT_NOTNULL(entry_string_1, "Expect entry strings to be nonnull");
	T_ASSERT_EQ(strcmp(entry_string_1, "CAFE"), 0,
		    "Expect entry strings to be CAFE");

	const char *entry_string_2 = os_map_entry(&basic_string_map,
						  &lookup_string_2);

	T_ASSERT_NE(entry_string_2, NULL, "Expect entry strings to be nonnull");
	T_ASSERT_EQ(strcmp(entry_string_2, "CAFE"), 0,
		    "Expect entry strings to be CAFE");

	T_ASSERT_EQ(entry_string_1, entry_string_2,
		    "Expect entry strings to be literally equal");

	os_map_destroy(&basic_string_map);
}

T_DECL(map_basic_128, "Make sure 64 bit map basics work",
        T_META("owner", "Core Darwin Daemons & Tools"))
{
    os_map_128_t basic_128_map;
    __block bool got_cafebabe = false;
    __block bool got_deadbeaf = false;
    uint64_t value;

    T_LOG("Start");

    // *** BASIC 64 bit key testing ***

    os_map_init(&basic_128_map, NULL);

    T_ASSERT_EQ(os_map_count(&basic_128_map), 0, "Expect map to be empty");

    os_map_128_key_t key;
    
    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    os_map_insert(&basic_128_map, key, (void *)0xCAFEBABE);
    
    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    os_map_insert(&basic_128_map, key, (void *)0xDEADBEEF);

    T_ASSERT_EQ(os_map_count(&basic_128_map), 2, "Expect map to have 2 entries");

    os_map_foreach(&basic_128_map, ^bool (os_map_128_key_t key, void *value){
        T_LOG("Foreach called for 0x%llx:0x%llx, 0x%llx",
	      (unsigned long long)key.x[0], (unsigned long long)key.x[1],
	      (unsigned long long)value);
        if (key.x[0] == 0xCAFECAFE && key.x[1] == 0xBABEBABE) {
            T_ASSERT_EQ(value, (void *)0xCAFEBABE,
			"Callback expect 0xCAFEBABE");
            got_cafebabe = true;
        } else if (key.x[0] == 0xDEADDEAD && key.x[1] == 0xBEEFBEEF) {
            T_ASSERT_EQ(value, (void *)0xDEADBEEF, "Callback expec 0xDEADBEEF");
            got_deadbeaf = true;
        } else {
            T_FAIL("Got unexpected callback 0x%llx:0x%llx, 0x%llx",
		   (unsigned long long)key.x[0], (unsigned long long)key.x[1],
		   (unsigned long long)value);
        }
        return true;
    });

    if (!got_cafebabe || !got_deadbeaf) {
        T_FAIL("Failed to get callback");
    }

    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "Find 1");

    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0xDEADBEEF, "Find 2");

    key.x[0] = 0xFF00F0F0;
    key.x[1] = 0xFF00F0F0;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "Find 3");
    
    key.x[0] = 0xFF00F0F0;
    key.x[1] = 0xBABEBABE;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "Find 4");
    
    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xFF00F0F0;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "Find 5");

    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    os_map_delete(&basic_128_map, key);

    T_ASSERT_EQ(os_map_count(&basic_128_map), 1,
		"Expect map to have 1 entries");

    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 1");

    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0xCAFEBABE, "After-delete find 2");

    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    os_map_delete(&basic_128_map, key);

    T_ASSERT_EQ(os_map_count(&basic_128_map), 0, "Expect map to be empty");

    key.x[0] = 0xDEADDEAD;
    key.x[1] = 0xBEEFBEEF;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete Find 3");

    key.x[0] = 0xCAFECAFE;
    key.x[1] = 0xBABEBABE;
    value = (uint64_t)os_map_find(&basic_128_map, key);
    T_ASSERT_EQ(value, (uint64_t)0x0, "After-delete find 4");

    os_map_destroy(&basic_128_map);
}
