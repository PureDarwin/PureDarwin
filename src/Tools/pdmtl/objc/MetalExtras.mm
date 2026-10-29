// Metal API that darling-metal lacks, as categories on its classes. Debug groups and signposts only label work for
// capture tools, which PD has none of. A command buffer runs in commit order, so enqueue has nothing to reserve
#import <Metal/MTLBlitCommandEncoderInternal.h>
#import <Metal/MTLCommandBufferInternal.h>
#import <Metal/MTLCommandQueueInternal.h>
#import <Metal/MTLComputeCommandEncoderInternal.h>
#import <Metal/MTLRenderCommandEncoderInternal.h>

@implementation MTLCommandQueueInternal (PDExtras)

// buffers here never retain their resources differently
- (id<MTLCommandBuffer>)commandBufferWithUnretainedReferences {
  return [self commandBuffer];
}

@end

@implementation MTLCommandBufferInternal (PDExtras)

- (void)enqueue {
}

- (void)pushDebugGroup:(NSString *)name {
  (void)name;
}

- (void)popDebugGroup {
}

@end

#define PD_DEBUG_GROUPS(cls)                       \
  @implementation cls (PDExtras)                   \
  -(void)pushDebugGroup : (NSString *)name {       \
    (void)name;                                    \
  }                                                \
  -(void)popDebugGroup {                           \
  }                                                \
  -(void)insertDebugSignpost : (NSString *)name {  \
    (void)name;                                    \
  }                                                \
  @end

PD_DEBUG_GROUPS(MTLRenderCommandEncoderInternal)
PD_DEBUG_GROUPS(MTLComputeCommandEncoderInternal)
PD_DEBUG_GROUPS(MTLBlitCommandEncoderInternal)

// descriptor and helper classes darling-metal has no declaration of: the properties Apple documents, copyable,
// so code that builds them (MoltenVK) links and can hand them back to the device
#include <dispatch/dispatch.h>

MTL_EXTERN NSString *const MTLCommandBufferEncoderInfoErrorKey = @"MTLCommandBufferEncoderInfoErrorKey";
MTL_EXTERN NSString *const MTLCommonCounterSetTimestamp = @"timestamp";
MTL_EXTERN NSString *const MTLCommonCounterTimestamp = @"GPUTimestamp";

@interface MTLArgumentDescriptor : NSObject <NSCopying>
@property(nonatomic) NSUInteger dataType, index, arrayLength, access, textureType, constantBlockAlignment;
+ (MTLArgumentDescriptor *)argumentDescriptor;
@end

@implementation MTLArgumentDescriptor
+ (MTLArgumentDescriptor *)argumentDescriptor {
  return [[[self alloc] init] autorelease];
}
- (id)copyWithZone:(NSZone *)zone {
  MTLArgumentDescriptor *c = [[[self class] allocWithZone:zone] init];
  c.dataType = _dataType, c.index = _index, c.arrayLength = _arrayLength, c.access = _access;
  c.textureType = _textureType, c.constantBlockAlignment = _constantBlockAlignment;
  return c;
}
@end

// indexed sub-descriptors made on first use, as Apple's *DescriptorArray classes do
@interface PDDescriptorArray : NSObject <NSCopying> {
  Class _cls;
  NSMutableDictionary *_items;
}
- (instancetype)initWithClass:(Class)cls;
- (id)objectAtIndexedSubscript:(NSUInteger)i;
- (void)setObject:(id)obj atIndexedSubscript:(NSUInteger)i;
@end

@implementation PDDescriptorArray
- (instancetype)initWithClass:(Class)cls {
  if ((self = [super init]))
    _cls = cls, _items = [[NSMutableDictionary alloc] init];
  return self;
}
- (void)dealloc {
  [_items release];
  [super dealloc];
}
- (id)objectAtIndexedSubscript:(NSUInteger)i {
  id o = _items[@(i)];
  if (!o) {
    o = [[[_cls alloc] init] autorelease];
    _items[@(i)] = o;
  }
  return o;
}
- (void)setObject:(id)obj atIndexedSubscript:(NSUInteger)i {
  if (obj)
    _items[@(i)] = [[obj copy] autorelease];
  else
    [_items removeObjectForKey:@(i)];
}
- (id)copyWithZone:(NSZone *)zone {
  PDDescriptorArray *c = [[PDDescriptorArray allocWithZone:zone] initWithClass:_cls];
  for (NSNumber *k in _items)
    c->_items[k] = [[_items[k] copy] autorelease];
  return c;
}
@end

@interface MTLBlitPassSampleBufferAttachmentDescriptor : NSObject <NSCopying>
@property(nonatomic, retain) id sampleBuffer;
@property(nonatomic) NSUInteger startOfEncoderSampleIndex, endOfEncoderSampleIndex;
@end

@implementation MTLBlitPassSampleBufferAttachmentDescriptor
- (void)dealloc {
  [_sampleBuffer release];
  [super dealloc];
}
- (id)copyWithZone:(NSZone *)zone {
  MTLBlitPassSampleBufferAttachmentDescriptor *c = [[[self class] allocWithZone:zone] init];
  c.sampleBuffer = _sampleBuffer, c.startOfEncoderSampleIndex = _startOfEncoderSampleIndex;
  c.endOfEncoderSampleIndex = _endOfEncoderSampleIndex;
  return c;
}
@end

@interface MTLBlitPassDescriptor : NSObject <NSCopying>
@property(nonatomic, readonly) PDDescriptorArray *sampleBufferAttachments;
+ (MTLBlitPassDescriptor *)blitPassDescriptor;
@end

@implementation MTLBlitPassDescriptor {
  PDDescriptorArray *_attachments;
}
+ (MTLBlitPassDescriptor *)blitPassDescriptor {
  return [[[self alloc] init] autorelease];
}
- (instancetype)init {
  if ((self = [super init]))
    _attachments = [[PDDescriptorArray alloc] initWithClass:[MTLBlitPassSampleBufferAttachmentDescriptor class]];
  return self;
}
- (void)dealloc {
  [_attachments release];
  [super dealloc];
}
- (PDDescriptorArray *)sampleBufferAttachments {
  return _attachments;
}
- (id)copyWithZone:(NSZone *)zone {
  MTLBlitPassDescriptor *c = [[[self class] allocWithZone:zone] init];
  [c->_attachments release];
  c->_attachments = [_attachments copy];
  return c;
}
@end

@interface MTLCaptureDescriptor : NSObject <NSCopying>
@property(nonatomic, retain) id captureObject;
@property(nonatomic) NSInteger destination;
@property(nonatomic, copy) NSURL *outputURL;
@end

@implementation MTLCaptureDescriptor
- (void)dealloc {
  [_captureObject release];
  [_outputURL release];
  [super dealloc];
}
- (id)copyWithZone:(NSZone *)zone {
  MTLCaptureDescriptor *c = [[[self class] allocWithZone:zone] init];
  c.captureObject = _captureObject, c.destination = _destination, c.outputURL = _outputURL;
  return c;
}
@end

@interface MTLCommandBufferDescriptor : NSObject <NSCopying>
@property(nonatomic) BOOL retainedReferences;
@property(nonatomic) NSUInteger errorOptions;
@property(nonatomic, retain) id logState;
@end

@implementation MTLCommandBufferDescriptor
- (instancetype)init {
  if ((self = [super init]))
    _retainedReferences = YES;
  return self;
}
- (void)dealloc {
  [_logState release];
  [super dealloc];
}
- (id)copyWithZone:(NSZone *)zone {
  MTLCommandBufferDescriptor *c = [[[self class] allocWithZone:zone] init];
  c.retainedReferences = _retainedReferences, c.errorOptions = _errorOptions, c.logState = _logState;
  return c;
}
@end

@interface MTLCounterSampleBufferDescriptor : NSObject <NSCopying>
@property(nonatomic, retain) id counterSet;
@property(nonatomic, copy) NSString *label;
@property(nonatomic) NSInteger storageMode;
@property(nonatomic) NSUInteger sampleCount;
@end

@implementation MTLCounterSampleBufferDescriptor
- (instancetype)init {
  if ((self = [super init]))
    _label = [@"" copy];
  return self;
}
- (void)dealloc {
  [_counterSet release];
  [_label release];
  [super dealloc];
}
- (id)copyWithZone:(NSZone *)zone {
  MTLCounterSampleBufferDescriptor *c = [[[self class] allocWithZone:zone] init];
  c.counterSet = _counterSet, c.label = _label, c.storageMode = _storageMode, c.sampleCount = _sampleCount;
  return c;
}
@end

// values for a function's constants, by index or name, each kept as its bytes
@interface MTLFunctionConstantValues : NSObject <NSCopying> {
@public
  NSMutableDictionary *_byIndex, *_byName;
}
- (void)setConstantValue:(const void *)value type:(NSUInteger)type atIndex:(NSUInteger)index;
- (void)setConstantValues:(const void *)values type:(NSUInteger)type withRange:(NSRange)range;
- (void)setConstantValue:(const void *)value type:(NSUInteger)type withName:(NSString *)name;
- (void)reset;
@end

// bytes of one value of an MTLDataType: scalars and vectors (Apple's numbering), 0 for types constants cannot have
static size_t constantBytes(NSUInteger type) {
  struct { NSUInteger first; size_t lane; } runs[] = {
    { 3, 4 },   // float .. float4
    { 16, 2 },  // half .. half4
    { 29, 4 },  // int .. int4
    { 33, 4 },  // uint .. uint4
    { 37, 2 },  // short .. short4
    { 41, 2 },  // ushort .. ushort4
    { 45, 1 },  // char .. char4
    { 49, 1 },  // uchar .. uchar4
    { 53, 1 },  // bool .. bool4
  };

  for (auto &r : runs) {
    if (type >= r.first && type < r.first + 4)
      return r.lane * (type - r.first + 1);
  }
  if (type == 81 || type == 85)  // long, ulong
    return 8;
  return 0;
}

@implementation MTLFunctionConstantValues
- (instancetype)init {
  if ((self = [super init]))
    _byIndex = [[NSMutableDictionary alloc] init], _byName = [[NSMutableDictionary alloc] init];
  return self;
}
- (void)dealloc {
  [_byIndex release];
  [_byName release];
  [super dealloc];
}
- (void)setConstantValue:(const void *)value type:(NSUInteger)type atIndex:(NSUInteger)index {
  size_t n = constantBytes(type);
  _byIndex[@(index)] = @[ @(type), [NSData dataWithBytes:value length:n ? n : 4] ];
}
- (void)setConstantValues:(const void *)values type:(NSUInteger)type withRange:(NSRange)range {
  size_t n = constantBytes(type) ? constantBytes(type) : 4;
  for (NSUInteger i = 0; i < range.length; i++)
    [self setConstantValue:(const uint8_t *)values + i * n type:type atIndex:range.location + i];
}
- (void)setConstantValue:(const void *)value type:(NSUInteger)type withName:(NSString *)name {
  size_t n = constantBytes(type);
  _byName[name] = @[ @(type), [NSData dataWithBytes:value length:n ? n : 4] ];
}
- (void)reset {
  [_byIndex removeAllObjects];
  [_byName removeAllObjects];
}
- (id)copyWithZone:(NSZone *)zone {
  MTLFunctionConstantValues *c = [[[self class] allocWithZone:zone] init];
  [c->_byIndex addEntriesFromDictionary:_byIndex];
  [c->_byName addEntriesFromDictionary:_byName];
  return c;
}
@end

@interface MTLHeapDescriptor : NSObject <NSCopying>
@property(nonatomic) NSUInteger size, resourceOptions, storageMode, cpuCacheMode, hazardTrackingMode, sparsePageSize;
@property(nonatomic) NSInteger type;
@end

@implementation MTLHeapDescriptor
- (id)copyWithZone:(NSZone *)zone {
  MTLHeapDescriptor *c = [[[self class] allocWithZone:zone] init];
  c.size = _size, c.resourceOptions = _resourceOptions, c.storageMode = _storageMode, c.cpuCacheMode = _cpuCacheMode;
  c.hazardTrackingMode = _hazardTrackingMode, c.sparsePageSize = _sparsePageSize, c.type = _type;
  return c;
}
@end

@interface MTLResidencySetDescriptor : NSObject <NSCopying>
@property(nonatomic, copy) NSString *label;
@property(nonatomic) NSUInteger initialCapacity;
@end

@implementation MTLResidencySetDescriptor
- (void)dealloc {
  [_label release];
  [super dealloc];
}
- (id)copyWithZone:(NSZone *)zone {
  MTLResidencySetDescriptor *c = [[[self class] allocWithZone:zone] init];
  c.label = _label, c.initialCapacity = _initialCapacity;
  return c;
}
@end

// the queue a shared event's notifications run on
@interface MTLSharedEventListener : NSObject
@property(nonatomic, readonly) dispatch_queue_t dispatchQueue;
- (instancetype)initWithDispatchQueue:(dispatch_queue_t)queue;
+ (MTLSharedEventListener *)sharedListener;
@end

@implementation MTLSharedEventListener
- (instancetype)init {
  return [self initWithDispatchQueue:dispatch_queue_create("MTLSharedEventListener", DISPATCH_QUEUE_SERIAL)];
}
- (instancetype)initWithDispatchQueue:(dispatch_queue_t)queue {
  if ((self = [super init])) {
    dispatch_retain(queue);
    _dispatchQueue = queue;
  }
  return self;
}
- (void)dealloc {
  dispatch_release(_dispatchQueue);
  [super dealloc];
}
+ (MTLSharedEventListener *)sharedListener {
  static MTLSharedEventListener *shared;
  static dispatch_once_t once;
  dispatch_once(&once, ^{ shared = [[MTLSharedEventListener alloc] init]; });
  return shared;
}
@end

@interface MTLBufferLayoutDescriptor2 : NSObject <NSCopying>
@property(nonatomic) NSUInteger stride, stepFunction, stepRate;
@end
@implementation MTLBufferLayoutDescriptor2
- (id)copyWithZone:(NSZone *)zone {
  MTLBufferLayoutDescriptor2 *c = [[[self class] allocWithZone:zone] init];
  c.stride = _stride, c.stepFunction = _stepFunction, c.stepRate = _stepRate;
  return c;
}
@end

@interface MTLAttributeDescriptor2 : NSObject <NSCopying>
@property(nonatomic) NSUInteger format, offset, bufferIndex;
@end
@implementation MTLAttributeDescriptor2
- (id)copyWithZone:(NSZone *)zone {
  MTLAttributeDescriptor2 *c = [[[self class] allocWithZone:zone] init];
  c.format = _format, c.offset = _offset, c.bufferIndex = _bufferIndex;
  return c;
}
@end

// a compute kernel's stage_in layout: attributes and buffer layouts like a vertex descriptor, plus the index buffer
@interface MTLStageInputOutputDescriptor : NSObject <NSCopying>
@property(nonatomic, readonly) PDDescriptorArray *layouts, *attributes;
@property(nonatomic) NSUInteger indexType, indexBufferIndex;
+ (MTLStageInputOutputDescriptor *)stageInputOutputDescriptor;
- (void)reset;
@end

@implementation MTLStageInputOutputDescriptor {
  PDDescriptorArray *_layouts, *_attributes;
}
+ (MTLStageInputOutputDescriptor *)stageInputOutputDescriptor {
  return [[[self alloc] init] autorelease];
}
- (instancetype)init {
  if ((self = [super init]))
    [self reset];
  return self;
}
- (void)reset {
  [_layouts release];
  [_attributes release];
  _layouts = [[PDDescriptorArray alloc] initWithClass:[MTLBufferLayoutDescriptor2 class]];
  _attributes = [[PDDescriptorArray alloc] initWithClass:[MTLAttributeDescriptor2 class]];
  _indexType = 0, _indexBufferIndex = 0;
}
- (void)dealloc {
  [_layouts release];
  [_attributes release];
  [super dealloc];
}
- (PDDescriptorArray *)layouts {
  return _layouts;
}
- (PDDescriptorArray *)attributes {
  return _attributes;
}
- (id)copyWithZone:(NSZone *)zone {
  MTLStageInputOutputDescriptor *c = [[[self class] allocWithZone:zone] init];
  [c->_layouts release];
  [c->_attributes release];
  c->_layouts = [_layouts copy], c->_attributes = [_attributes copy];
  c.indexType = _indexType, c.indexBufferIndex = _indexBufferIndex;
  return c;
}
@end

// events and fences: work runs in commit order on the device's thread, a signal is a step after the work before it
// and a wait holds that thread until the value arrives. Fences order encoders, which that order already does
#import <Metal/MTLDeviceInternal.h>
#import <Metal/MTLComputePipeline.h>
#import <Metal/MTLRenderPipeline.h>
#include <indium/pd-sync.hpp>
#include <mach/mach_time.h>
#include <IOKit/IOKitLib.h>
#include <condition_variable>
#include <mutex>
#include <vector>

typedef void (^PDSharedEventNotificationBlock)(id event, uint64_t value);

@interface PDEvent : NSObject {
@public
  std::mutex _mu;
  std::condition_variable _cv;
  uint64_t _value;
  std::vector<std::pair<uint64_t, std::pair<MTLSharedEventListener *, PDSharedEventNotificationBlock>>> _notify;
}
@property(nonatomic, retain) id<MTLDevice> device;
@property(nonatomic, copy) NSString *label;
- (instancetype)initWithDevice:(id<MTLDevice>)device;
- (void)signalValue:(uint64_t)value;
- (BOOL)waitForValue:(uint64_t)value timeoutMS:(uint64_t)ms;
@end

@implementation PDEvent
- (instancetype)initWithDevice:(id<MTLDevice>)device {
  if ((self = [super init]))
    _device = [device retain];
  return self;
}
- (void)dealloc {
  for (auto &n: _notify) {
    [n.second.first release];
    [n.second.second release];
  }
  [_device release];
  [_label release];
  [super dealloc];
}
// listeners whose value is reached run once, on their listener's queue
- (void)signalValue:(uint64_t)value {
  decltype(_notify) due;
  {
    std::lock_guard<std::mutex> lock(_mu);
    _value = value;
    for (auto it = _notify.begin(); it != _notify.end();) {
      if (it->first <= value) {
        due.push_back(*it);
        it = _notify.erase(it);
      } else {
        ++it;
      }
    }
  }
  _cv.notify_all();
  for (auto &n: due) {
    PDSharedEventNotificationBlock block = n.second.second;
    [self retain];
    dispatch_async(n.second.first.dispatchQueue, ^{
      block(self, value);
      [block release];
      [self release];
    });
    [n.second.first release];
  }
}
- (BOOL)waitForValue:(uint64_t)value timeoutMS:(uint64_t)ms {
  std::unique_lock<std::mutex> lock(_mu);
  if (ms == UINT64_MAX) {
    _cv.wait(lock, [&] { return _value >= value; });
    return YES;
  }
  return _cv.wait_for(lock, std::chrono::milliseconds(ms), [&] { return _value >= value; });
}
@end

// the CPU side of MTLSharedEvent, a handle names the event within this process
@interface PDSharedEvent : PDEvent
@property(nonatomic) uint64_t signaledValue;
@end

@interface MTLSharedEventHandle : NSObject <NSSecureCoding>
@property(nonatomic, readonly) NSString *label;
@property(nonatomic, readonly) PDSharedEvent *event;
- (instancetype)initWithEvent:(PDSharedEvent *)event;
@end

@implementation MTLSharedEventHandle
+ (BOOL)supportsSecureCoding {
  return YES;
}
- (instancetype)initWithEvent:(PDSharedEvent *)event {
  if ((self = [super init]))
    _event = [event retain];
  return self;
}
- (void)dealloc {
  [_event release];
  [super dealloc];
}
- (NSString *)label {
  return _event.label;
}
- (void)encodeWithCoder:(NSCoder *)coder {
  (void)coder;
}
- (instancetype)initWithCoder:(NSCoder *)coder {
  (void)coder;
  [self release];
  return nil;
}
@end

@implementation PDSharedEvent
- (uint64_t)signaledValue {
  std::lock_guard<std::mutex> lock(_mu);
  return _value;
}
- (void)setSignaledValue:(uint64_t)value {
  [self signalValue:value];
}
- (void)notifyListener:(MTLSharedEventListener *)listener atValue:(uint64_t)value block:(PDSharedEventNotificationBlock)block {
  BOOL now;
  {
    std::lock_guard<std::mutex> lock(_mu);
    now = _value >= value;
    if (!now)
      _notify.push_back({value, {[listener retain], [block copy]}});
  }
  if (!now) return;
  [self retain];
  PDSharedEventNotificationBlock b = [block copy];
  dispatch_async(listener.dispatchQueue, ^{
    b(self, value);
    [b release];
    [self release];
  });
}
- (BOOL)waitUntilSignaledValue:(uint64_t)value timeoutMS:(uint64_t)milliseconds {
  return [self waitForValue:value timeoutMS:milliseconds];
}
- (MTLSharedEventHandle *)newSharedEventHandle {
  return [[MTLSharedEventHandle alloc] initWithEvent:self];
}
@end

@interface PDFence : NSObject
@property(nonatomic, retain) id<MTLDevice> device;
@property(nonatomic, copy) NSString *label;
@end

@implementation PDFence
- (void)dealloc {
  [_device release];
  [_label release];
  [super dealloc];
}
@end

@implementation MTLCommandBufferInternal (PDEvents)
- (void)encodeSignalEvent:(PDEvent *)event value:(uint64_t)value {
  [event retain];
  // the CPU may be watching: what the work before the signal wrote has to be there when it sees the value
  IndiumPD::addStep(self.indiumCommandBuffer, [event, value] {
    IndiumPD::finishSubmitted();
    [event signalValue:value];
    [event release];
  });
}
- (void)encodeWaitForEvent:(PDEvent *)event value:(uint64_t)value {
  [event retain];
  IndiumPD::addStep(self.indiumCommandBuffer, [event, value] {
    [event waitForValue:value timeoutMS:UINT64_MAX];
    [event release];
  });
}
- (void)addScheduledHandler:(void (^)(id<MTLCommandBuffer>))block {
  void (^b)(id<MTLCommandBuffer>) = [block copy];
  id<MTLCommandBuffer> cb = [self retain];
  self.indiumCommandBuffer->addScheduledHandler([b, cb](std::shared_ptr<Indium::CommandBuffer>) {
    b(cb);
    [b release];
    [cb release];
  });
}
- (id<MTLBlitCommandEncoder>)blitCommandEncoderWithDescriptor:(id)descriptor {
  (void)descriptor;
  return [self blitCommandEncoder];
}
// errors are reported by status and error, nothing extra is kept per encoder
- (NSUInteger)errorOptions {
  return 0;
}
- (id)logs {
  return nil;
}
@end

@implementation MTLCommandQueueInternal (PDDescriptors)
- (id<MTLCommandBuffer>)commandBufferWithDescriptor:(MTLCommandBufferDescriptor *)descriptor {
  (void)descriptor;
  return [self commandBuffer];
}
@end

#define PD_FENCES(cls)                                                           \
  @implementation cls (PDFences)                                                 \
  -(void)updateFence : (id)fence {                                               \
    (void)fence;                                                                 \
  }                                                                              \
  -(void)waitForFence : (id)fence {                                              \
    (void)fence;                                                                 \
  }                                                                              \
  -(void)useResource : (id)resource usage : (NSUInteger)usage {                  \
    (void)resource, (void)usage;                                                 \
  }                                                                              \
  -(void)useResources : (const id *)r count : (NSUInteger)n usage : (NSUInteger)u { \
    (void)r, (void)n, (void)u;                                                   \
  }                                                                              \
  -(void)useHeap : (id)heap {                                                    \
    (void)heap;                                                                  \
  }                                                                              \
  @end

// every resource is resident, and encoders already run one after another in order
PD_FENCES(MTLComputeCommandEncoderInternal)
PD_FENCES(MTLBlitCommandEncoderInternal)
PD_FENCES(MTLRenderCommandEncoderInternal)

@implementation MTLRenderCommandEncoderInternal (PDStages)
- (void)updateFence:(id)fence afterStages:(NSUInteger)stages {
  (void)fence, (void)stages;
}
- (void)waitForFence:(id)fence beforeStages:(NSUInteger)stages {
  (void)fence, (void)stages;
}
- (void)useResource:(id)resource usage:(NSUInteger)usage stages:(NSUInteger)stages {
  (void)resource, (void)usage, (void)stages;
}
- (void)useResources:(const id *)r count:(NSUInteger)n usage:(NSUInteger)u stages:(NSUInteger)s {
  (void)r, (void)n, (void)u, (void)s;
}
@end

// what the device can do: the Vulkan executor's limits, and no for features it has no path for. Timestamps are the
// CPU's clock on both sides
@implementation MTLDeviceInternal (PDLimits)
- (BOOL)hasUnifiedMemory {
  return YES;
}
// what MoltenVK asks of a device while it sets up (seen in its message log on a Mac): the executor filters 32 bit
// floats linearly, resolves 32 bit formats by copying and has the BC formats in its table
- (BOOL)supports32BitFloatFiltering {
  return YES;
}
- (BOOL)supports32BitMSAA {
  return NO;
}
- (BOOL)supportsBCTextureCompression {
  return YES;
}
- (BOOL)isHeadless {
  return NO;
}
- (BOOL)isLowPower {
  return NO;
}
- (BOOL)isRemovable {
  return NO;
}
- (NSUInteger)location {
  return 0;
}
- (NSUInteger)locationNumber {
  return 0;
}
- (uint64_t)maxTransferRate {
  return 0;
}
// the registry entry of the PCI display device the window server draws through, so callers can
// match the Metal device to the IOKit GPU and to CGDirectDisplayCopyCurrentMetalDevice
- (uint64_t)registryID {
  static uint64_t cached;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(MACH_PORT_NULL, IOServiceMatching("IOPCIDevice"), &iterator) != KERN_SUCCESS)
      return;
    io_registry_entry_t entry;
    while (cached == 0 && (entry = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
      CFTypeRef classCode = IORegistryEntryCreateCFProperty(entry, CFSTR("class-code"), kCFAllocatorDefault, 0);
      if (classCode != NULL && CFGetTypeID(classCode) == CFDataGetTypeID() && CFDataGetLength((CFDataRef)classCode) >= 4) {
        uint32_t code = 0;
        CFDataGetBytes((CFDataRef)classCode, CFRangeMake(0, 4), (UInt8 *)&code);
        if ((code >> 16) == 0x03)
          IORegistryEntryGetRegistryEntryID(entry, &cached);
      }
      if (classCode != NULL)
        CFRelease(classCode);
      IOObjectRelease(entry);
    }
    IOObjectRelease(iterator);
  });
  return cached;
}
- (uint32_t)peerGroupID {
  return 0;
}
- (uint32_t)peerIndex {
  return 0;
}
- (uint32_t)peerCount {
  return 1;
}
- (uint64_t)recommendedMaxWorkingSetSize {
  return 2ull << 30;
}
- (NSUInteger)currentAllocatedSize {
  return 0;
}
- (NSUInteger)maxBufferLength {
  return 1ull << 30;
}
- (NSUInteger)maxThreadgroupMemoryLength {
  return 32768;
}
- (MTLSize)maxThreadsPerThreadgroup {
  return MTLSizeMake(1024, 1024, 64);
}
- (NSUInteger)maxArgumentBufferSamplerCount {
  return 16;
}
// tier 1: argument buffers through argument encoders
- (NSUInteger)argumentBuffersSupport {
  return 0;
}
- (NSUInteger)readWriteTextureSupport {
  return 0;
}
- (NSUInteger)maximumConcurrentCompilationTaskCount {
  return 1;
}
- (BOOL)shouldMaximizeConcurrentCompilation {
  return NO;
}
- (void)setShouldMaximizeConcurrentCompilation:(BOOL)value {
  (void)value;
}
- (BOOL)isDepth24Stencil8PixelFormatSupported {
  return NO;
}
- (BOOL)areProgrammableSamplePositionsSupported {
  return NO;
}
- (BOOL)areRasterOrderGroupsSupported {
  return NO;
}
- (BOOL)areBarycentricCoordsSupported {
  return NO;
}
- (BOOL)supportsShaderBarycentricCoordinates {
  return NO;
}
- (BOOL)supportsPullModelInterpolation {
  return NO;
}
- (BOOL)supportsQueryTextureLOD {
  return NO;
}
- (BOOL)supportsDynamicLibraries {
  return NO;
}
- (BOOL)supportsRenderDynamicLibraries {
  return NO;
}
- (BOOL)supportsFunctionPointers {
  return NO;
}
- (BOOL)supportsFunctionPointersFromRender {
  return NO;
}
- (BOOL)supportsRaytracing {
  return NO;
}
- (BOOL)supportsRaytracingFromRender {
  return NO;
}
- (BOOL)supportsPrimitiveMotionBlur {
  return NO;
}
- (BOOL)supportsPlacementSparse {
  return NO;
}
- (BOOL)supportsTextureSampleCount:(NSUInteger)count {
  return count == 1;
}
- (BOOL)supportsVertexAmplificationCount:(NSUInteger)count {
  return count == 1;
}
- (BOOL)supportsRasterizationRateMapWithLayerCount:(NSUInteger)count {
  (void)count;
  return NO;
}
- (BOOL)supportsCounterSampling:(NSUInteger)samplingPoint {
  (void)samplingPoint;
  return NO;
}
- (NSArray *)counterSets {
  return @[];
}
// macOS GPU family 1 and 2 feature sets (10000-10005)
- (BOOL)supportsFeatureSet:(NSUInteger)featureSet {
  return featureSet >= 10000 && featureSet <= 10005;
}
- (void)sampleTimestamps:(uint64_t *)cpuTimestamp gpuTimestamp:(uint64_t *)gpuTimestamp {
  uint64_t now = mach_absolute_time();
  if (cpuTimestamp) *cpuTimestamp = now;
  if (gpuTimestamp) *gpuTimestamp = now;
}
- (uint64_t)queryTimestampFrequency {
  mach_timebase_info_data_t tb;
  mach_timebase_info(&tb);
  return (uint64_t)(1e9 * tb.denom / tb.numer);
}
- (id<MTLCommandQueue>)newCommandQueueWithMaxCommandBufferCount:(NSUInteger)count {
  (void)count;
  return [self newCommandQueue];
}
- (id<MTLCommandQueue>)newCommandQueueWithDescriptor:(id)descriptor {
  (void)descriptor;
  return [self newCommandQueue];
}
- (id)newEvent {
  return [[PDEvent alloc] initWithDevice:self];
}
- (id)newSharedEvent {
  return [[PDSharedEvent alloc] initWithDevice:self];
}
- (id)newSharedEventWithHandle:(MTLSharedEventHandle *)handle {
  return [handle.event retain];
}
- (id)newFence {
  PDFence *f = [[PDFence alloc] init];
  f.device = self;
  return f;
}
@end

// the asynchronous pipeline and function makers, done in place: their callers already run them off their own thread
@implementation MTLDeviceInternal (PDAsync)
- (id<MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor
                                                           options:(MTLPipelineOption)options
                                                        reflection:(id *)reflection
                                                             error:(NSError **)error {
  (void)options;
  if (reflection) *reflection = nil;
  return [self newRenderPipelineStateWithDescriptor:descriptor error:error];
}
- (void)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor
                           completionHandler:(void (^)(id<MTLRenderPipelineState>, NSError *))handler {
  NSError *error = nil;
  id<MTLRenderPipelineState> state = [self newRenderPipelineStateWithDescriptor:descriptor error:&error];
  handler(state, error);
  [state release];
}
- (void)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor
                                     options:(MTLPipelineOption)options
                           completionHandler:(void (^)(id<MTLRenderPipelineState>, id, NSError *))handler {
  (void)options;
  NSError *error = nil;
  id<MTLRenderPipelineState> state = [self newRenderPipelineStateWithDescriptor:descriptor error:&error];
  handler(state, nil, error);
  [state release];
}
- (void)newComputePipelineStateWithFunction:(id<MTLFunction>)function
                          completionHandler:(void (^)(id<MTLComputePipelineState>, NSError *))handler {
  NSError *error = nil;
  id<MTLComputePipelineState> state = [self newComputePipelineStateWithFunction:function error:&error];
  handler(state, error);
  [state release];
}
- (void)newComputePipelineStateWithFunction:(id<MTLFunction>)function
                                    options:(MTLPipelineOption)options
                          completionHandler:(void (^)(id<MTLComputePipelineState>, id, NSError *))handler {
  NSError *error = nil;
  id<MTLComputePipelineState> state = [self newComputePipelineStateWithFunction:function options:options
                                                                     reflection:nil error:&error];
  handler(state, nil, error);
  [state release];
}
- (void)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor
                                      options:(MTLPipelineOption)options
                            completionHandler:(void (^)(id<MTLComputePipelineState>, id, NSError *))handler {
  NSError *error = nil;
  id<MTLComputePipelineState> state = [self newComputePipelineStateWithDescriptor:descriptor options:options
                                                                       reflection:nil error:&error];
  handler(state, nil, error);
  [state release];
}
@end

// GPU capture: PD has no capture tool, so scopes only bracket work and a capture never starts
#import <Metal/MTLCaptureManager.h>

@interface PDCaptureScope : NSObject
@property(nonatomic, retain) id<MTLDevice> device;
@property(nonatomic, retain) id<MTLCommandQueue> commandQueue;
@property(nonatomic, copy) NSString *label;
@end

@implementation PDCaptureScope
- (void)dealloc {
  [_device release];
  [_commandQueue release];
  [_label release];
  [super dealloc];
}
- (void)beginScope {
}
- (void)endScope {
}
@end

@implementation MTLCaptureManager (PDCapture)
static id pdDefaultCaptureScope;

+ (MTLCaptureManager *)sharedCaptureManager {
  static MTLCaptureManager *shared;
  static dispatch_once_t once;
  dispatch_once(&once, ^{ shared = [[MTLCaptureManager alloc] init]; });
  return shared;
}
- (id)newCaptureScopeWithDevice:(id<MTLDevice>)device {
  PDCaptureScope *s = [[PDCaptureScope alloc] init];
  s.device = device;
  return s;
}
- (id)newCaptureScopeWithCommandQueue:(id<MTLCommandQueue>)queue {
  PDCaptureScope *s = [[PDCaptureScope alloc] init];
  s.commandQueue = queue, s.device = queue.device;
  return s;
}
- (id)defaultCaptureScope {
  return pdDefaultCaptureScope;
}
- (void)setDefaultCaptureScope:(id)scope {
  [scope retain];
  [pdDefaultCaptureScope release];
  pdDefaultCaptureScope = scope;
}
- (BOOL)isCapturing {
  return NO;
}
- (BOOL)supportsDestination:(NSInteger)destination {
  (void)destination;
  return NO;
}
- (BOOL)startCaptureWithDescriptor:(MTLCaptureDescriptor *)descriptor error:(NSError **)error {
  (void)descriptor;
  if (error) *error = [NSError errorWithDomain:@"MTLCaptureErrorDomain" code:1 userInfo:@{NSLocalizedDescriptionKey: @"capture is not supported"}];
  return NO;
}
- (void)startCaptureWithDevice:(id<MTLDevice>)device {
  (void)device;
}
- (void)startCaptureWithCommandQueue:(id<MTLCommandQueue>)queue {
  (void)queue;
}
- (void)startCaptureWithScope:(id)scope {
  (void)scope;
}
- (void)stopCapture {
}
@end

// store actions given at the end of a pass (MTLStoreActionUnknown in its descriptor). Options only matter to
// custom sample positions, which this device has none of
@implementation MTLRenderCommandEncoderInternal (PDStoreActions)
- (void)setColorStoreAction:(NSUInteger)action atIndex:(NSUInteger)index {
  IndiumPD::setStoreAction(self.encoder, (int)index, static_cast<Indium::StoreAction>(action));
}
- (void)setDepthStoreAction:(NSUInteger)action {
  IndiumPD::setStoreAction(self.encoder, IndiumPD::storeDepth, static_cast<Indium::StoreAction>(action));
}
- (void)setStencilStoreAction:(NSUInteger)action {
  IndiumPD::setStoreAction(self.encoder, IndiumPD::storeStencil, static_cast<Indium::StoreAction>(action));
}
- (void)setColorStoreActionOptions:(NSUInteger)options atIndex:(NSUInteger)index {
  (void)options, (void)index;
}
- (void)setDepthStoreActionOptions:(NSUInteger)options {
  (void)options;
}
- (void)setStencilStoreActionOptions:(NSUInteger)options {
  (void)options;
}
@end

// heaps: one arena range. Placement heaps put resources where they are told (aliasing each other), automatic ones
// one after another. Resources say which heap they came from
#import <Metal/MTLBufferInternal.h>
#import <Metal/MTLTextureInternal.h>
#import <Metal/MTLDescriptorsInternal.h>
#import <Metal/MTLTextureDescriptor.h>
#include <objc/runtime.h>

typedef struct {
  NSUInteger size, align;
} PDSizeAndAlign;

static const NSUInteger pdHeapAlign = 256;
static char pdHeapKey, pdHeapOffsetKey, pdAliasableKey, pdSurfaceKey;

static NSUInteger pdAlignUp(NSUInteger n, NSUInteger a) {
  return (n + a - 1) & ~(a - 1);
}

@interface PDHeap : NSObject {
@public
  std::shared_ptr<void> _range;
  NSUInteger _next;
}
@property(nonatomic, retain) id<MTLDevice> device;
@property(nonatomic, copy) NSString *label;
@property(nonatomic) NSUInteger size, storageMode, cpuCacheMode, hazardTrackingMode, resourceOptions;
@property(nonatomic) NSInteger type;
@end

@implementation PDHeap
- (void)dealloc {
  [_device release];
  [_label release];
  [super dealloc];
}
- (NSUInteger)usedSize {
  return _type == 1 ? _size : _next; // placement heaps are wholly in use
}
- (NSUInteger)currentAllocatedSize {
  return _size;
}
- (NSUInteger)maxAvailableSizeWithAlignment:(NSUInteger)alignment {
  NSUInteger at = pdAlignUp(_next, alignment ? alignment : 1);
  return at < _size ? _size - at : 0;
}
- (NSUInteger)setPurgeableState:(NSUInteger)state {
  (void)state;
  return 1; // MTLPurgeableStateNonVolatile
}
- (id<MTLBuffer>)placeBuffer:(NSUInteger)length options:(NSUInteger)options offset:(NSUInteger)offset {
  if (offset + length > _size) return nil;
  auto device = ((MTLDeviceInternal *)_device).device;
  MTLBufferInternal *b = [[MTLBufferInternal alloc] initWithBuffer:IndiumPD::newPlacedBuffer(device, _range, offset, length)
                                                           device:_device resourceOptions:options];
  objc_setAssociatedObject(b, &pdHeapKey, self, OBJC_ASSOCIATION_RETAIN);
  objc_setAssociatedObject(b, &pdHeapOffsetKey, @(offset), OBJC_ASSOCIATION_RETAIN);
  return b;
}
- (id<MTLTexture>)placeTexture:(MTLTextureDescriptor *)descriptor offset:(NSUInteger)offset {
  Indium::TextureDescriptor d = [descriptor asIndiumDescriptor];
  if (offset + IndiumPD::textureStorageSize(d) > _size) return nil;
  auto device = ((MTLDeviceInternal *)_device).device;
  MTLTextureInternal *t = [[MTLTextureInternal alloc] initWithTexture:IndiumPD::newPlacedTexture(device, _range, offset, d)
                                                               device:_device resourceOptions:descriptor.resourceOptions];
  [t setHeap:self offset:offset];
  return t;
}
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(NSUInteger)options offset:(NSUInteger)offset {
  return [self placeBuffer:length options:options offset:offset];
}
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor offset:(NSUInteger)offset {
  return [self placeTexture:descriptor offset:offset];
}
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(NSUInteger)options {
  NSUInteger at = pdAlignUp(_next, pdHeapAlign);
  id<MTLBuffer> b = [self placeBuffer:length options:options offset:at];
  if (b) _next = at + length;
  return b;
}
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor {
  NSUInteger at = pdAlignUp(_next, pdHeapAlign);
  id<MTLTexture> t = [self placeTexture:descriptor offset:at];
  if (t) _next = at + IndiumPD::textureStorageSize([descriptor asIndiumDescriptor]);
  return t;
}
@end

@implementation MTLDeviceInternal (PDHeaps)
- (PDSizeAndAlign)heapBufferSizeAndAlignWithLength:(NSUInteger)length options:(NSUInteger)options {
  (void)options;
  return (PDSizeAndAlign){pdAlignUp(length ? length : 1, pdHeapAlign), pdHeapAlign};
}
- (PDSizeAndAlign)heapTextureSizeAndAlignWithDescriptor:(MTLTextureDescriptor *)descriptor {
  return (PDSizeAndAlign){pdAlignUp(IndiumPD::textureStorageSize([descriptor asIndiumDescriptor]), pdHeapAlign), pdHeapAlign};
}
- (id)newHeapWithDescriptor:(MTLHeapDescriptor *)descriptor {
  if (!descriptor.size) return nil;
  PDHeap *h = [[PDHeap alloc] init];
  h.device = self;
  h.size = pdAlignUp(descriptor.size, pdHeapAlign), h.type = descriptor.type, h.storageMode = descriptor.storageMode;
  h.cpuCacheMode = descriptor.cpuCacheMode, h.hazardTrackingMode = descriptor.hazardTrackingMode;
  h.resourceOptions = descriptor.cpuCacheMode | (descriptor.storageMode << 4) | (descriptor.hazardTrackingMode << 8);
  h->_range = IndiumPD::newArenaRange(h.size);
  return h;
}
@end

// a texture buffer: one row of width texels over an MTLBuffer's bytes, made by newTextureWithDescriptor:offset:bytesPerRow:
@implementation MTLTextureDescriptor (PDTextureBuffer)
+ (MTLTextureDescriptor *)textureBufferDescriptorWithPixelFormat:(MTLPixelFormat)pixelFormat width:(NSUInteger)width
                                                 resourceOptions:(MTLResourceOptions)resourceOptions
                                                           usage:(MTLTextureUsage)usage {
  MTLTextureDescriptor *d = [[[MTLTextureDescriptor alloc] init] autorelease];
  d.textureType = MTLTextureTypeTextureBuffer;
  d.pixelFormat = pixelFormat;
  d.width = width, d.height = 1, d.depth = 1;
  d.mipmapLevelCount = 1, d.arrayLength = 1, d.sampleCount = 1;
  // storage, cache and hazard tracking modes are read from the options
  d.resourceOptions = resourceOptions;
  d.usage = usage;
  return d;
}
@end

// a texture over a buffer's bytes (linear images), its rows have to be the tight rows the executor reads. A texture
// buffer is one row, whatever row pitch the caller passes
@implementation MTLBufferInternal (PDTextures)
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor offset:(NSUInteger)offset
                               bytesPerRow:(NSUInteger)bytesPerRow {
  Indium::TextureDescriptor d = [descriptor asIndiumDescriptor];
  if (descriptor.textureType == MTLTextureTypeTextureBuffer)
    d.height = 1, d.depth = 1, bytesPerRow = IndiumPD::tightRowBytes(d.pixelFormat, d.width);
  if (bytesPerRow != IndiumPD::tightRowBytes(d.pixelFormat, d.width)) {
    NSLog(@"Metal: a texture over a buffer needs %zu-byte rows, not %lu", IndiumPD::tightRowBytes(d.pixelFormat, d.width),
          (unsigned long)bytesPerRow);
    return nil;
  }
  // a texture buffer's bytes are its row, the storage size rounds a slice up
  size_t need = descriptor.textureType == MTLTextureTypeTextureBuffer ? bytesPerRow : IndiumPD::textureStorageSize(d);
  if (offset + need > self.length) return nil;
  MTLTextureInternal *t = [[MTLTextureInternal alloc] initWithTexture:IndiumPD::newBufferTexture(self.buffer, offset, d)
                                                               device:self.device resourceOptions:descriptor.resourceOptions];
  [t setBuffer:self offset:offset bytesPerRow:bytesPerRow];
  return t;
}
- (id)heap {
  return objc_getAssociatedObject(self, &pdHeapKey);
}
- (NSUInteger)heapOffset {
  return [objc_getAssociatedObject(self, &pdHeapOffsetKey) unsignedIntegerValue];
}
- (void)makeAliasable {
  objc_setAssociatedObject(self, &pdAliasableKey, @YES, OBJC_ASSOCIATION_RETAIN);
}
- (BOOL)isAliasable {
  return [objc_getAssociatedObject(self, &pdAliasableKey) boolValue];
}
- (NSUInteger)setPurgeableState:(NSUInteger)state {
  (void)state;
  return 1;
}
- (NSUInteger)allocatedSize {
  return self.length;
}
@end

// rows are tight, so any alignment of the format's own pixels will do
@implementation MTLDeviceInternal (PDLinear)
- (NSUInteger)minimumLinearTextureAlignmentForPixelFormat:(NSUInteger)format {
  (void)format;
  return 1;
}
- (NSUInteger)minimumTextureBufferAlignmentForPixelFormat:(NSUInteger)format {
  (void)format;
  return 1;
}
@end

@interface PDSurfaceHolder : NSObject {
@public
  void *_surface;
  NSUInteger _plane;
}
- (id)surface;
- (NSUInteger)plane;
@end

@implementation MTLTextureInternal (PDResource)
- (void)makeAliasable {
  objc_setAssociatedObject(self, &pdAliasableKey, @YES, OBJC_ASSOCIATION_RETAIN);
}
- (BOOL)isAliasable {
  return [objc_getAssociatedObject(self, &pdAliasableKey) boolValue];
}
- (NSUInteger)setPurgeableState:(NSUInteger)state {
  (void)state;
  return 1;
}
- (id)iosurface {
  return [(PDSurfaceHolder *)objc_getAssociatedObject(self, &pdSurfaceKey) surface];
}
- (NSUInteger)iosurfacePlane {
  return [(PDSurfaceHolder *)objc_getAssociatedObject(self, &pdSurfaceKey) plane];
}
@end

// IOSurface-backed textures: the arena copy is refreshed when the surface seed moves and written back after a GPU write
#include <dlfcn.h>

typedef struct PDSurfaceFns {
  void *(*retain)(void *);
  void (*release)(void *);
  int (*lock)(void *, uint32_t, uint32_t *);
  int (*unlock)(void *, uint32_t, uint32_t *);
  uint32_t (*seed)(void *);
  size_t (*planeCount)(void *);
  void *(*base)(void *);
  size_t (*bpr)(void *);
  size_t (*height)(void *);
  void *(*planeBase)(void *, size_t);
  size_t (*planeBpr)(void *, size_t);
  size_t (*planeHeight)(void *, size_t);
} PDSurfaceFns;

static const PDSurfaceFns *pdSurfaceFns(void) {
  static PDSurfaceFns f;
  static dispatch_once_t once;
  static BOOL ok;
  dispatch_once(&once, ^{
    void *h = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_LAZY);
    if (!h) h = RTLD_DEFAULT;
    f.retain = (void *(*)(void *))dlsym(RTLD_DEFAULT, "CFRetain");
    f.release = (void (*)(void *))dlsym(RTLD_DEFAULT, "CFRelease");
    f.lock = (int (*)(void *, uint32_t, uint32_t *))dlsym(h, "IOSurfaceLock");
    f.unlock = (int (*)(void *, uint32_t, uint32_t *))dlsym(h, "IOSurfaceUnlock");
    f.seed = (uint32_t (*)(void *))dlsym(h, "IOSurfaceGetSeed");
    f.planeCount = (size_t (*)(void *))dlsym(h, "IOSurfaceGetPlaneCount");
    f.base = (void *(*)(void *))dlsym(h, "IOSurfaceGetBaseAddress");
    f.bpr = (size_t (*)(void *))dlsym(h, "IOSurfaceGetBytesPerRow");
    f.height = (size_t (*)(void *))dlsym(h, "IOSurfaceGetHeight");
    f.planeBase = (void *(*)(void *, size_t))dlsym(h, "IOSurfaceGetBaseAddressOfPlane");
    f.planeBpr = (size_t (*)(void *, size_t))dlsym(h, "IOSurfaceGetBytesPerRowOfPlane");
    f.planeHeight = (size_t (*)(void *, size_t))dlsym(h, "IOSurfaceGetHeightOfPlane");
    ok = f.retain && f.release && f.lock && f.unlock && f.seed && f.planeCount && f.base && f.bpr && f.height &&
         f.planeBase && f.planeBpr && f.planeHeight;
  });
  return ok ? &f : NULL;
}

@implementation PDSurfaceHolder
- (id)surface {
  return (__bridge id)_surface;
}
- (NSUInteger)plane {
  return _plane;
}
- (void)dealloc {
  if (_surface) pdSurfaceFns()->release(_surface);
  [super dealloc];
}
@end

namespace {
struct PDSurfaceState {
  const PDSurfaceFns *f;
  void *surface;
  size_t plane, rows;
  uint32_t seen = UINT32_MAX;
  ~PDSurfaceState() { f->release(surface); }

  uint8_t *rowsAt(size_t *bpr, size_t *height) {
    if (f->planeCount(surface) == 0) {
      *bpr = f->bpr(surface), *height = f->height(surface);
      return (uint8_t *)f->base(surface);
    }
    *bpr = f->planeBpr(surface, plane), *height = f->planeHeight(surface, plane);
    return (uint8_t *)f->planeBase(surface, plane);
  }

  // copies min(rows, heights) rows of min(row bytes) between surface and arena
  void copy(uint8_t *tex, size_t texBpr, bool toTexture) {
    size_t bpr, height;
    uint8_t *src = rowsAt(&bpr, &height);
    size_t n = std::min(rows, height), w = std::min(bpr, texBpr);
    for (size_t y = 0; y < n; y++) {
      if (toTexture) memcpy(tex + y * texBpr, src + y * bpr, w);
      else memcpy(src + y * bpr, tex + y * texBpr, w);
    }
  }
};
}

@implementation MTLDeviceInternal (PDSurfaces)
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor iosurface:(struct __IOSurface *)iosurface
                                     plane:(NSUInteger)plane {
  const PDSurfaceFns *f = pdSurfaceFns();
  if (!f || !iosurface) return nil;
  size_t planes = f->planeCount(iosurface);
  if ((planes == 0 && plane != 0) || (planes != 0 && plane >= planes)) return nil;
  MTLTextureInternal *t = (MTLTextureInternal *)[self newTextureWithDescriptor:descriptor];
  if (!t) return nil;

  auto state = std::make_shared<PDSurfaceState>();
  state->f = f, state->surface = f->retain(iosurface), state->plane = plane, state->rows = descriptor.height;
  auto link = std::make_shared<IndiumPD::SurfaceLink>();
  link->pull = [state](uint8_t *bytes, size_t bpr) {
    uint32_t seed = state->f->seed(state->surface);
    if (seed == state->seen) return false;
    state->f->lock(state->surface, 1, NULL);
    state->copy(bytes, bpr, true);
    state->f->unlock(state->surface, 1, NULL);
    state->seen = seed;
    return true;
  };
  link->push = [state](const uint8_t *bytes, size_t bpr) {
    state->f->lock(state->surface, 0, NULL);
    state->copy(const_cast<uint8_t *>(bytes), bpr, false);
    uint32_t seed = 0;
    state->f->unlock(state->surface, 0, &seed);
    state->seen = seed;
  };
  if (!IndiumPD::linkSurface(t.texture, link)) {
    [t release];
    return nil;
  }

  PDSurfaceHolder *holder = [PDSurfaceHolder new];
  holder->_surface = f->retain(iosurface), holder->_plane = plane;
  objc_setAssociatedObject(t, &pdSurfaceKey, holder, OBJC_ASSOCIATION_RETAIN);
  [holder release];
  return t;
}
@end


// function constants: reflection from !air.function_constants, specialised functions built by indium-pd
#import <Metal/MTLLibraryInternal.h>

@interface MTLFunctionConstant : NSObject
@property(nonatomic, copy) NSString *name;
@property(nonatomic) NSUInteger type, index;
@property(nonatomic) BOOL required;
@end

@implementation MTLFunctionConstant
- (void)dealloc {
  [_name release];
  [super dealloc];
}
@end

// MSL type names to MTLDataType (Apple's numbering)
static NSUInteger dataTypeNamed(const std::string &t) {
  static const struct { const char *base; NSUInteger first; } bases[] = {
    { "float", 3 }, { "half", 16 }, { "int", 29 }, { "uint", 33 }, { "short", 37 }, { "ushort", 41 },
    { "char", 45 }, { "uchar", 49 }, { "bool", 53 },
  };

  if (t == "long") return 81;
  if (t == "ulong") return 85;
  for (auto &b : bases) {
    size_t n = strlen(b.base);
    if (t.compare(0, n, b.base)) continue;
    if (t.size() == n) return b.first;
    if (t.size() == n + 1 && t[n] >= '2' && t[n] <= '4') return b.first + (t[n] - '1');
  }
  return 0;
}

static char pdFunctionConstantsKey;

@implementation MTLFunctionInternal (PDConstants)
- (NSDictionary *)functionConstantsDictionary {
  NSDictionary *d = objc_getAssociatedObject(self, &pdFunctionConstantsKey);

  if (d) return d;
  NSMutableDictionary *m = [NSMutableDictionary dictionary];
  for (auto &c : IndiumPD::functionConstants(self.function)) {
    MTLFunctionConstant *fc = [[MTLFunctionConstant alloc] init];
    fc.name = [NSString stringWithUTF8String:c.name.c_str()];
    fc.type = dataTypeNamed(c.type), fc.index = c.index, fc.required = c.required;
    m[fc.name] = fc;
    [fc release];
  }
  objc_setAssociatedObject(self, &pdFunctionConstantsKey, m, OBJC_ASSOCIATION_RETAIN);
  return m;
}
@end

@implementation MTLLibraryInternal (PDConstants)
- (id<MTLFunction>)newFunctionWithName:(NSString *)name constantValues:(MTLFunctionConstantValues *)values
                                 error:(NSError **)error {
  Ivar iv = class_getInstanceVariable([MTLLibraryInternal class], "_library");
  auto lib = *(std::shared_ptr<Indium::Library> *)((char *)self + ivar_getOffset(iv));
  std::vector<IndiumPD::ConstantValue> v;

  for (NSNumber *k in values->_byIndex) {
    NSData *b = values->_byIndex[k][1];
    v.push_back({ (uint32_t)k.unsignedIntegerValue, "", { (const uint8_t *)b.bytes, (const uint8_t *)b.bytes + b.length } });
  }
  for (NSString *k in values->_byName) {
    NSData *b = values->_byName[k][1];
    v.push_back({ ~0u, k.UTF8String, { (const uint8_t *)b.bytes, (const uint8_t *)b.bytes + b.length } });
  }

  auto fn = IndiumPD::specializeFunction(lib, name.UTF8String, v);
  if (!fn) {
    if (error)
      *error = [NSError errorWithDomain:@"MTLLibraryErrorDomain" code:5
                               userInfo:@{ NSLocalizedDescriptionKey : [NSString stringWithFormat:@"no function %@", name] }];
    return nil;
  }
  return [[MTLFunctionInternal alloc] initWithFunction:fn device:self.device];
}
- (void)newFunctionWithName:(NSString *)name constantValues:(MTLFunctionConstantValues *)values
          completionHandler:(void (^)(id<MTLFunction>, NSError *))handler {
  NSError *e = nil;
  id<MTLFunction> f = [self newFunctionWithName:name constantValues:values error:&e];

  handler(f, e);
  [f release];
}
@end
