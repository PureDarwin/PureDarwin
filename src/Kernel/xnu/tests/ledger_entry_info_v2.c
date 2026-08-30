#include <darwintest.h>
#include <darwintest_utils.h>
#include <sys/types.h>
#include <TargetConditionals.h>

#include "../osfmk/kern/ledger.h"
extern int ledger(int cmd, caddr_t arg1, caddr_t arg2, caddr_t arg3);

T_DECL(ledger_entry_v2,
    "test the LEDGER_ENTRY_INFO_V2 command of ledger() syscal",
    T_META_LTEPHASE(LTE_POSTINIT),
    T_META_OWNER("skwok2"),
    T_META_TAG_VM_PREFERRED)
{
	struct ledger_info li;
	int64_t ledger_count;
	struct ledger_entry_info_v2 *lei_v2 = NULL;
	bool retrieved_lifetime_max = false;
	size_t malloc_size = 0;

	T_QUIET; T_ASSERT_EQ(ledger(LEDGER_INFO,
	    (caddr_t)(uintptr_t)getpid(),
	    (caddr_t)&li,
	    NULL),
	    0,
	    "ledger(LEDGER_INFO)");

	ledger_count = li.li_entries;
	T_QUIET; T_ASSERT_GT(ledger_count, 0, "no ledger entry available");

	malloc_size = (size_t)ledger_count * sizeof(struct ledger_entry_info_v2);
	lei_v2 = (struct ledger_entry_info_v2 *)malloc(malloc_size);
	T_QUIET; T_ASSERT_NE(lei_v2, NULL, "malloc(ledger_entry_info_v2) of size %u", malloc_size);


	T_ASSERT_GE(ledger(LEDGER_ENTRY_INFO_V2,
	    (caddr_t)(uintptr_t)getpid(),
	    (caddr_t)lei_v2,
	    (caddr_t)&ledger_count),
	    0,
	    "ledger(LEDGER_ENTRY_INFO_V2)");

	for (int i = 0; i < ledger_count; i++) {
		if (lei_v2[i].lei_lifetime_max != -1) {
			retrieved_lifetime_max = true;
			break;
		}
	}

	free(lei_v2);

	if (retrieved_lifetime_max) {
		T_PASS("successfully retrieved at least one entry which support lifetime max");
	} else {
		T_FAIL("couldn't read any lifetime max value");
	}
}
