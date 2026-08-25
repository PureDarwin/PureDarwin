#if defined(__x86_64__)
#include <x86_64/libc-features.h>
#elif defined(__arm64__) || defined(__aarch64__)
#include <arm64/libc-features.h>
#else
#error Unsupported PureDarwin SDK architecture
#endif
