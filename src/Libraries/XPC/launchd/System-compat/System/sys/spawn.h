/*
 * PureDarwin: real macOS SDKs ship <System/sys/spawn.h> as a copy/symlink of
 * <sys/spawn.h> under the "System.framework private headers" root, which
 * this build doesn't replicate. core.c includes it via that path directly;
 * just forward to the real header instead of reproducing Apple's SDK layout.
 */
#include <sys/spawn.h>
