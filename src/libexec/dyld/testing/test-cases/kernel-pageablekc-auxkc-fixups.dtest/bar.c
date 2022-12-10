
#include "../kernel-test-runner.h"

extern int pageableExport();
__typeof(&pageableExport) pageableExportPtr = &pageableExport;

int bar() {
	return pageableExportPtr() + 2;
}

extern int pageableExportDirect();

// Test direct pointer fixups to the pageable KC.  On x86_64 these would be emitted as just
// a branch relocation so we needed to synthesize a stub
__attribute__((constructor))
int testDirectToPageable(const TestRunnerFunctions* funcs) {
    LOG("testDirectToPageable(): start");
    // The pageable returned 42
    int v = pageableExportDirect();
    if ( v != 42 ) {
        FAIL("pageableExportDirect() returned %d vs expected 42", v);
    }
    LOG("testDirectToPageable(): end");
    return 0;
}
