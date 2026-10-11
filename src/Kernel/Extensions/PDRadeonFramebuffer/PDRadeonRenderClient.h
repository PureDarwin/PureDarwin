#pragma once

#include <IOKit/IOUserClient.h>
#include "PDRadeonCard.h"

class PDRadeonRenderClient : public IOUserClient
{
    OSDeclareDefaultStructors(PDRadeonRenderClient);
    RadeonCard *fCard;
    IOLock *fLock;
    task_t fTask;
    uint32_t fPresentedHandle;
    struct Buffer { uint32_t offset, size; IOMemoryDescriptor *memory; bool owned; } fBuffers[512];
    void releaseBuffers(void);
    void restoreScanout(void);
public:
    bool initWithTask(task_t task, void *token, UInt32 type) override;
    bool start(IOService *provider) override;
    void free(void) override;
    IOReturn clientClose(void) override;
    IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
        IOExternalMethodDispatch *, OSObject *, void *) override;
    IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
        IOMemoryDescriptor **memory) override;
};
