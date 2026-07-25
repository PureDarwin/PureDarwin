#include <mach-o/loader.h>
#include <mach-o/dyld_images.h>

#include "AllImages.h"

void setNotifyMonitoringDyldMain(void (*)())
{
}

void setNotifyMonitoringDyld(void (*)(bool unloading, unsigned imageCount,
                                      const struct mach_header* loadAddresses[],
                                      const char* imagePaths[]))
{
}

namespace dyld3 {

void AllImages::notifyMonitorMain()
{
}

void AllImages::notifyMonitorLoads(const Array<LoadedImage>&)
{
}

void AllImages::notifyMonitorUnloads(const Array<LoadedImage>&)
{
}

} // namespace dyld3
