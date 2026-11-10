// DYLD_INSERT_LIBRARIES probe for Firefox on PD: samples the main thread's stack every few
// seconds and counts CVDisplayLink callbacks, logging both to /dev/console
#include <CoreVideo/CoreVideo.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>

#define DYLD_INTERPOSE(_replacement, _replacee) \
    __attribute__((used)) static struct { const void *replacement; const void *replacee; } _interpose_##_replacee \
    __attribute__((section("__DATA,__interpose"))) = { (const void *)(unsigned long)&_replacement, (const void *)(unsigned long)&_replacee };

static int consoleFd = -1;

static void say(const char *format, ...) {
    char line[512];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (consoleFd < 0)
        consoleFd = open("/dev/console", O_WRONLY | O_NOCTTY);
    if (n > 0)
        write(consoleFd >= 0 ? consoleFd : 2, line, n < (int)sizeof(line) ? n : (int)sizeof(line) - 1);
}

static void symbolize(const char *tag, uintptr_t pc) {
    Dl_info info;
    if (dladdr((void *)pc, &info) && info.dli_fname) {
        const char *image = strrchr(info.dli_fname, '/');
        say("FFPROBE %s %s+0x%lx %s+0x%lx\n", tag, image ? image + 1 : info.dli_fname,
            (unsigned long)(pc - (uintptr_t)info.dli_fbase), info.dli_sname ? info.dli_sname : "?",
            (unsigned long)(info.dli_saddr ? pc - (uintptr_t)info.dli_saddr : 0));
    } else
        say("FFPROBE %s 0x%lx\n", tag, (unsigned long)pc);
}

static mach_port_t mainThread;
static volatile unsigned long linkTicks, linkCallbacks;

static void sampleThread(mach_port_t thread, const char *tag);

static void sampleMain(void) {
    sampleThread(mainThread, "main");
}

// every thread with its pthread name, to see where the compositor and renderer wait
static void sampleAll(void) {
    thread_act_array_t threads;
    mach_msg_type_number_t n = 0;
    mach_port_t self = mach_thread_self();
    if (task_threads(mach_task_self(), &threads, &n) != KERN_SUCCESS)
        return;
    for (mach_msg_type_number_t i = 0; i < n; i++) {
        char name[64] = "";
        pthread_t pt = pthread_from_mach_thread_np(threads[i]);
        if (pt)
            pthread_getname_np(pt, name, sizeof(name));
        if (threads[i] != self) {
            char tag[80];
            snprintf(tag, sizeof(tag), "T%u[%s]", i, name);
            sampleThread(threads[i], tag);
        }
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)threads, n * sizeof(thread_act_t));
}

static void sampleThread(mach_port_t thread, const char *tag) {
    x86_thread_state64_t state;
    mach_msg_type_number_t count = x86_THREAD_STATE64_COUNT;
    mach_port_t mainThread = thread;
    if (thread_suspend(mainThread) != KERN_SUCCESS)
        return;
    if (thread_get_state(mainThread, x86_THREAD_STATE64, (thread_state_t)&state, &count) == KERN_SUCCESS) {
        symbolize(tag, state.__rip);
        uintptr_t *fp = (uintptr_t *)state.__rbp;
        for (int i = 0; i < 14 && fp != NULL && ((uintptr_t)fp & 7) == 0; i++) {
            uintptr_t ret = fp[1];
            if (ret == 0)
                break;
            symbolize(tag, ret);
            uintptr_t *next = (uintptr_t *)fp[0];
            if (next <= fp)
                break;
            fp = next;
        }
    }
    thread_resume(mainThread);
}

static void *sampler(void *arg) {
    for (int round = 0;; round++) {
        sleep(round < 3 ? 8 : 15);
        say("FFPROBE --- sample %d in %d, display link callbacks %lu\n", round, getpid(), linkCallbacks);
        if (round == 4)
            sampleAll();
        else
            sampleMain();
    }
    return NULL;
}

__attribute__((constructor)) static void start(void) {
    pthread_t thread;
    mainThread = mach_thread_self();
    pthread_create(&thread, NULL, sampler, NULL);
    say("FFPROBE loaded in %d\n", getpid());
}

typedef struct { CVDisplayLinkOutputCallback callback; void *context; } Hook;
static Hook hooks[16];
static int hookCount;

static CVReturn hookedCallback(CVDisplayLinkRef link, const CVTimeStamp *now, const CVTimeStamp *out,
                               CVOptionFlags flags, CVOptionFlags *outFlags, void *context) {
    Hook *hook = context;
    linkCallbacks++;
    if (linkCallbacks == 1 || linkCallbacks % 300 == 0)
        say("FFPROBE display link callback %lu into %p\n", linkCallbacks, (void *)hook->callback);
    return hook->callback(link, now, out, flags, outFlags, hook->context);
}

static CVReturn my_SetOutputCallback(CVDisplayLinkRef link, CVDisplayLinkOutputCallback callback, void *context) {
    say("FFPROBE CVDisplayLinkSetOutputCallback %p cb %p\n", (void *)link, (void *)callback);
    symbolize("cb", (uintptr_t)callback);
    if (callback == NULL || hookCount >= 16)
        return CVDisplayLinkSetOutputCallback(link, callback, context);
    Hook *hook = &hooks[hookCount++];
    hook->callback = callback;
    hook->context = context;
    return CVDisplayLinkSetOutputCallback(link, hookedCallback, hook);
}
DYLD_INTERPOSE(my_SetOutputCallback, CVDisplayLinkSetOutputCallback)

static CVReturn my_Start(CVDisplayLinkRef link) {
    CVReturn r = CVDisplayLinkStart(link);
    say("FFPROBE CVDisplayLinkStart %p -> %d\n", (void *)link, r);
    return r;
}
DYLD_INTERPOSE(my_Start, CVDisplayLinkStart)

static CVReturn my_Stop(CVDisplayLinkRef link) {
    say("FFPROBE CVDisplayLinkStop %p\n", (void *)link);
    return CVDisplayLinkStop(link);
}
DYLD_INTERPOSE(my_Stop, CVDisplayLinkStop)

static CVReturn my_CreateActive(CVDisplayLinkRef *out) {
    CVReturn r = CVDisplayLinkCreateWithActiveCGDisplays(out);
    say("FFPROBE CVDisplayLinkCreateWithActiveCGDisplays -> %d %p\n", r, out ? (void *)*out : NULL);
    return r;
}
DYLD_INTERPOSE(my_CreateActive, CVDisplayLinkCreateWithActiveCGDisplays)

static CVReturn my_CreateDisplay(CGDirectDisplayID display, CVDisplayLinkRef *out) {
    CVReturn r = CVDisplayLinkCreateWithCGDisplay(display, out);
    say("FFPROBE CVDisplayLinkCreateWithCGDisplay %u -> %d\n", display, r);
    return r;
}
DYLD_INTERPOSE(my_CreateDisplay, CVDisplayLinkCreateWithCGDisplay)

// what Gecko's compositor reaches: surfaces, layer contents and commits, counted and sampled
#include <objc/runtime.h>
#include <objc/message.h>
#include <IOSurface/IOSurface.h>

static volatile unsigned long surfaces, locks, contentsSets, sublayerAdds, commits;

static IOSurfaceRef my_IOSurfaceCreate(CFDictionaryRef props) {
    IOSurfaceRef s = IOSurfaceCreate(props);
    if (++surfaces <= 5)
        say("FFPROBE IOSurfaceCreate %p (%lu) %zux%zu\n", (void *)s, surfaces, s ? IOSurfaceGetWidth(s) : 0, s ? IOSurfaceGetHeight(s) : 0);
    return s;
}
DYLD_INTERPOSE(my_IOSurfaceCreate, IOSurfaceCreate)

static kern_return_t my_IOSurfaceLock(IOSurfaceRef s, IOSurfaceLockOptions o, uint32_t *seed) {
    locks++;
    return IOSurfaceLock(s, o, seed);
}
DYLD_INTERPOSE(my_IOSurfaceLock, IOSurfaceLock)

static IMP origSetContents, origAddSublayer, origCommit;

static void backtraceHere(const char *tag) {
    uintptr_t *fp = __builtin_frame_address(0);
    for (int i = 0; i < 10 && fp != NULL && ((uintptr_t)fp & 7) == 0; i++) {
        if (fp[1] == 0)
            break;
        symbolize(tag, fp[1]);
        uintptr_t *next = (uintptr_t *)fp[0];
        if (next <= fp)
            break;
        fp = next;
    }
}

static volatile unsigned long surfaceContents;

static void hookSetContents(id self, SEL _cmd, id value) {
    contentsSets++;
    if (value != nil && strcmp(object_getClassName(value), "O2Image") != 0 && ++surfaceContents <= 6) {
        say("FFPROBE setContents %s on %s %p\n", object_getClassName(value), object_getClassName(self), (void *)self);
        backtraceHere("contents");
    }
    ((void (*)(id, SEL, id))origSetContents)(self, _cmd, value);
}

static void hookAddSublayer(id self, SEL _cmd, id layer) {
    if (++sublayerAdds <= 12) {
        say("FFPROBE addSublayer %s %p to %s %p\n", layer ? object_getClassName(layer) : "nil", (void *)layer, object_getClassName(self), (void *)self);
        if (layer == nil)
            backtraceHere("nilsub");
    }
    ((void (*)(id, SEL, id))origAddSublayer)(self, _cmd, layer);
}

static void hookCommit(id self, SEL _cmd) {
    commits++;
    ((void (*)(id, SEL))origCommit)(self, _cmd);
}

typedef struct { double x, y, w, h; } PRect;

static void dumpLayer(id layer, int depth) {
    if (layer == nil || depth > 8)
        return;
    PRect frame = ((PRect (*)(id, SEL))objc_msgSend_stret)(layer, sel_registerName("frame"));
    BOOL flipped = ((BOOL (*)(id, SEL))objc_msgSend)(layer, sel_registerName("isGeometryFlipped"));
    id contents = ((id (*)(id, SEL))objc_msgSend)(layer, sel_registerName("contents"));
    id sublayers = ((id (*)(id, SEL))objc_msgSend)(layer, sel_registerName("sublayers"));
    unsigned long count = ((unsigned long (*)(id, SEL))objc_msgSend)(sublayers, sel_registerName("count"));
    say("FFPROBE layer %*s%s %.0f,%.0f %.0fx%.0f flipped %d contents %s subs %lu\n", depth * 2, "", object_getClassName(layer),
        frame.x, frame.y, frame.w, frame.h, flipped, contents ? object_getClassName(contents) : "-", count);
    for (unsigned long i = 0; i < count && i < 12; i++)
        dumpLayer(((id (*)(id, SEL, unsigned long))objc_msgSend)(sublayers, sel_registerName("objectAtIndex:"), i), depth + 1);
}

static void dumpView(id view, int depth) {
    if (view == nil || depth > 8)
        return;
    PRect frame = ((PRect (*)(id, SEL))objc_msgSend_stret)(view, sel_registerName("frame"));
    BOOL flipped = ((BOOL (*)(id, SEL))objc_msgSend)(view, sel_registerName("isFlipped"));
    id layer = ((id (*)(id, SEL))objc_msgSend)(view, sel_registerName("layer"));
    say("FFPROBE view %*s%s %.0f,%.0f %.0fx%.0f flipped %d layer %s\n", depth * 2, "", object_getClassName(view), frame.x, frame.y, frame.w, frame.h, flipped, layer ? object_getClassName(layer) : "-");
    id subviews = ((id (*)(id, SEL))objc_msgSend)(view, sel_registerName("subviews"));
    unsigned long count = ((unsigned long (*)(id, SEL))objc_msgSend)(subviews, sel_registerName("count"));
    for (unsigned long i = 0; i < count && i < 12; i++)
        dumpView(((id (*)(id, SEL, unsigned long))objc_msgSend)(subviews, sel_registerName("objectAtIndex:"), i), depth + 1);
}

static void dumpWindows(void *ctx) {
    id app = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
    id windows = ((id (*)(id, SEL))objc_msgSend)(app, sel_registerName("windows"));
    unsigned long count = ((unsigned long (*)(id, SEL))objc_msgSend)(windows, sel_registerName("count"));
    for (unsigned long i = 0; i < count; i++) {
        id window = ((id (*)(id, SEL, unsigned long))objc_msgSend)(windows, sel_registerName("objectAtIndex:"), i);
        id frameView = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)(window, sel_registerName("contentView")), sel_registerName("superview"));
        say("FFPROBE window %lu %s\n", i, object_getClassName(window));
        dumpView(frameView, 0);
    }
    for (unsigned long i = 0; i < count; i++) {
        id window = ((id (*)(id, SEL, unsigned long))objc_msgSend)(windows, sel_registerName("objectAtIndex:"), i);
        id content = ((id (*)(id, SEL))objc_msgSend)(window, sel_registerName("contentView"));
        dumpLayer(((id (*)(id, SEL))objc_msgSend)(content, sel_registerName("layer")), 0);
    }
}

static void *counters(void *arg) {
    sleep(5);
    Class layer = objc_getClass("CALayer"), transaction = objc_getClass("CATransaction");
    if (layer) {
        origSetContents = method_setImplementation(class_getInstanceMethod(layer, sel_registerName("setContents:")), (IMP)hookSetContents);
        origAddSublayer = method_setImplementation(class_getInstanceMethod(layer, sel_registerName("addSublayer:")), (IMP)hookAddSublayer);
    }
    if (transaction)
        origCommit = method_setImplementation(class_getClassMethod(transaction, sel_registerName("commit")), (IMP)hookCommit);
    say("FFPROBE hooks in, CALayer %p CATransaction %p\n", (void *)layer, (void *)transaction);
    for (int round = 0;; round++) {
        sleep(10);
        if (round == 4 && strstr(getprogname(), "firefox") != NULL)
            dispatch_async_f(dispatch_get_main_queue(), NULL, dumpWindows);
        say("FFPROBE counts %d: surfaces %lu locks %lu contents %lu (surface contents %lu) sublayers %lu commits %lu displaylink %lu\n",
            getpid(), surfaces, locks, contentsSets, surfaceContents, sublayerAdds, commits, linkCallbacks);
    }
    return NULL;
}

__attribute__((constructor)) static void startCounters(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, counters, NULL);
}
