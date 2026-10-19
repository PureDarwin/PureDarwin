// NSRunningApplication.m - a running process seen as an application
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSRunningApplication.h>
#import <AppKit/NSWorkspace.h>
#import <AppKit/NSImage.h>
#import <Foundation/Foundation.h>
#include <errno.h>
#include <libproc.h>
#include <signal.h>
#include <sys/sysctl.h>
#include <unistd.h>

// the bundle that holds an executable at X.app/Contents/MacOS/X, nil for a bare tool
static NSString *bundlePathForExecutable(NSString *executable)
{
    NSString *macos = [executable stringByDeletingLastPathComponent];
    NSString *contents = [macos stringByDeletingLastPathComponent];

    if (![[macos lastPathComponent] isEqualToString:@"MacOS"] || ![[contents lastPathComponent] isEqualToString:@"Contents"])
        return nil;
    return [contents stringByDeletingLastPathComponent];
}

static NSString *executablePathForPid(pid_t pid)
{
    char path[PROC_PIDPATHINFO_MAXSIZE];

    if (proc_pidpath(pid, path, sizeof(path)) <= 0)
        return nil;
    return [NSString stringWithUTF8String:path];
}

// the process start time, from the kernel's proc table
static NSDate *launchDateForPid(pid_t pid)
{
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    struct kinfo_proc info;
    size_t len = sizeof(info);

    if (sysctl(mib, 4, &info, &len, NULL, 0) != 0 || len == 0)
        return nil;
    return [NSDate dateWithTimeIntervalSince1970:info.kp_proc.p_starttime.tv_sec + info.kp_proc.p_starttime.tv_usec / 1e6];
}

@implementation NSRunningApplication

@synthesize processIdentifier = _pid;

- (instancetype)_initWithPid:(pid_t)pid
{
    NSString *executable = executablePathForPid(pid);
    NSString *bundlePath;
    NSBundle *bundle;

    if (executable == nil) {
        [self release];
        return nil;
    }
    if ((self = [super init]) == nil)
        return nil;
    _pid = pid;
    _executableURL = [[NSURL fileURLWithPath:executable] retain];
    bundlePath = bundlePathForExecutable(executable);
    bundle = bundlePath ? [NSBundle bundleWithPath:bundlePath] : nil;
    if (bundle != nil) {
        _bundleURL = [[NSURL fileURLWithPath:bundlePath] retain];
        _bundleIdentifier = [[bundle bundleIdentifier] copy];
        _localizedName = [([bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"] ?: [bundle objectForInfoDictionaryKey:@"CFBundleName"]) copy];
    }
    if (_localizedName == nil)
        _localizedName = [(bundlePath ? [[bundlePath lastPathComponent] stringByDeletingPathExtension] : [executable lastPathComponent]) copy];
    _launchDate = [launchDateForPid(pid) retain];
    return self;
}

- (void)dealloc
{
    [_bundleIdentifier release];
    [_bundleURL release];
    [_executableURL release];
    [_localizedName release];
    [_launchDate release];
    [super dealloc];
}

+ (NSRunningApplication *)currentApplication
{
    static NSRunningApplication *current;

    if (current == nil)
        current = [[self alloc] _initWithPid:getpid()];
    return current;
}

+ (instancetype)runningApplicationWithProcessIdentifier:(pid_t)pid
{
    if (pid == getpid())
        return [self currentApplication];
    return [[[self alloc] _initWithPid:pid] autorelease];
}

// every process whose bundle has this identifier, by walking the process list
+ (NSArray *)runningApplicationsWithBundleIdentifier:(NSString *)bundleIdentifier
{
    NSMutableArray *result = [NSMutableArray array];
    int count = proc_listallpids(NULL, 0);
    pid_t *pids;

    if (count <= 0 || bundleIdentifier == nil)
        return result;
    pids = calloc((size_t)count + 16, sizeof(pid_t));
    count = proc_listallpids(pids, (int)(((size_t)count + 16) * sizeof(pid_t)));
    for (int i = 0; i < count; i++) {
        NSString *executable = executablePathForPid(pids[i]);
        NSString *bundlePath = executable ? bundlePathForExecutable(executable) : nil;

        if (bundlePath && [[[NSBundle bundleWithPath:bundlePath] bundleIdentifier] isEqualToString:bundleIdentifier]) {
            NSRunningApplication *app = [self runningApplicationWithProcessIdentifier:pids[i]];
            if (app != nil)
                [result addObject:app];
        }
    }
    free(pids);
    return result;
}

+ (void)terminateAutomaticallyTerminableApplications
{
}

- (NSString *)bundleIdentifier
{
    return _bundleIdentifier;
}

- (NSURL *)bundleURL
{
    return _bundleURL;
}

- (NSURL *)executableURL
{
    return _executableURL;
}

- (NSString *)localizedName
{
    return _localizedName;
}

- (NSDate *)launchDate
{
    return _launchDate;
}

- (NSImage *)icon
{
    return [[NSWorkspace sharedWorkspace] iconForFile:[(_bundleURL ?: _executableURL) path]];
}

- (NSInteger)executableArchitecture
{
#if defined(__x86_64__)
    return 0x01000007;
#else
    return 0x0100000c;
#endif
}

- (BOOL)_isSelf
{
    return _pid == getpid();
}

- (NSApplicationActivationPolicy)activationPolicy
{
    if ([self _isSelf] && NSApp != nil)
        return [NSApp activationPolicy];
    return _bundleURL ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyProhibited;
}

- (BOOL)isActive
{
    return [self _isSelf] && [NSApp isActive];
}

- (BOOL)isHidden
{
    return [self _isSelf] && [NSApp isHidden];
}

- (BOOL)isTerminated
{
    return kill(_pid, 0) != 0 && errno == ESRCH;
}

- (BOOL)isFinishedLaunching
{
    return [self _isSelf] ? [NSApp isRunning] : ![self isTerminated];
}

- (BOOL)ownsMenuBar
{
    return [self isActive];
}

// only this process can be brought forward from here, another app owns its own windows
- (BOOL)activateWithOptions:(NSApplicationActivationOptions)options
{
    if (![self _isSelf])
        return NO;
    [NSApp activateIgnoringOtherApps:(options & NSApplicationActivateIgnoringOtherApps) != 0];
    return YES;
}

- (BOOL)activateFromApplication:(NSRunningApplication *)application options:(NSApplicationActivationOptions)options
{
    return [self activateWithOptions:options];
}

- (BOOL)activate
{
    return [self activateWithOptions:NSApplicationActivateIgnoringOtherApps];
}

- (BOOL)hide
{
    if (![self _isSelf])
        return NO;
    [NSApp hide:nil];
    return YES;
}

- (BOOL)unhide
{
    if (![self _isSelf])
        return NO;
    [NSApp unhide:nil];
    return YES;
}

- (BOOL)terminate
{
    if ([self _isSelf]) {
        [NSApp terminate:nil];
        return YES;
    }
    return kill(_pid, SIGTERM) == 0;
}

- (BOOL)forceTerminate
{
    return kill(_pid, SIGKILL) == 0;
}

- (NSUInteger)hash
{
    return (NSUInteger)_pid;
}

- (BOOL)isEqual:(id)other
{
    return [other isKindOfClass:[NSRunningApplication class]] && [(NSRunningApplication *)other processIdentifier] == _pid;
}

@end
