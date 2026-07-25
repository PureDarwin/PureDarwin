#include <cstddef>
#include <cstdlib>
#include <cstdint>

extern "C" void
pd_bootstrap_aligned_free(void *ptr)
{
    std::free(ptr);
}

extern "C" __attribute__((visibility("hidden"))) int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
    if (memptr == nullptr || alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0) {
        return 22; /* EINVAL */
    }

    if (alignment > 16) {
        return 12; /* ENOMEM - cannot honor free() contract above malloc alignment */
    }

    void *base = std::malloc(size);
    if (base == nullptr) {
        return 12;
    }

    *memptr = base;
    return 0;
}

namespace libunwind {

bool
checkKeyMgrRegisteredFDEs(unsigned long, void *&)
{
    return false;
}

}
