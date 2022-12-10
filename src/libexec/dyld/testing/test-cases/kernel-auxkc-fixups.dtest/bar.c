
#include "../kernel-test-runner.h"

extern int kernelExport();
__typeof(&kernelExport) kernelExportPtr = &kernelExport;

int bar() {
	return kernelExportPtr() + 2;
}

extern int kernelExportDirect();

// Test direct pointer fixups to the kernel.  On x86_64 these would be emitted as just
// a branch relocation so we needed to synthesize a stub
__attribute__((constructor))
int testDirectToKernel(const TestRunnerFunctions* funcs) {
    LOG("testDirectToKernel(): start");
    // The kernel returned 42
    int v = kernelExportDirect();
    if ( v != 42 ) {
        FAIL("kernelExportDirect() returned %d vs expected 42", v);
    }
    LOG("testDirectToKernel(): end");
    return 0;
}
