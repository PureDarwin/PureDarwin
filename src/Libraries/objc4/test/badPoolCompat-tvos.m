// Run test badPool as if it were built with an old SDK.

// TEST_CONFIG MEM=mrc OS=appletvos,appletvsimulator ARCH=x86_64,arm64
// TEST_CRASHES
// TEST_CFLAGS -DOLD=1 -Xlinker -platform_version -Xlinker tvos -Xlinker 9.0 -Xlinker 9.0 -mtvos-version-min=9.0

/*
TEST_BUILD_OUTPUT
ld: warning: passed two min versions.*for platform.*
END

TEST_RUN_OUTPUT
objc\[\d+\]: Invalid or prematurely-freed autorelease pool 0x[0-9a-fA-f]+\. Set a breakpoint .* Proceeding anyway .*
OK: badPool.m
END
*/

#include "badPool.m"
