/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSTask.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSFileHandle.h>
#import <Foundation/NSPipe.h>
#import <Foundation/NSException.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSProcessInfo.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

NSString * const NSTaskDidTerminateNotification = @"NSTaskDidTerminateNotification";

extern char **environ;

/* Accepts what -setStandardOutput: takes: an NSFileHandle, or an NSPipe whose
 * matching end is used. */
static int _descriptorForStream(id stream, BOOL forWriting) {
    if (stream == nil) {
        return -1;
    }
    if ([stream isKindOfClass:[NSPipe class]]) {
        NSFileHandle *handle = forWriting ? [stream fileHandleForWriting]
                                          : [stream fileHandleForReading];
        return [handle fileDescriptor];
    }
    if ([stream isKindOfClass:[NSFileHandle class]]) {
        return [stream fileDescriptor];
    }
    return -1;
}

/* Builds a NULL-terminated char ** from an array of NSStrings. The strings are
 * strdup'd because the array may be released before the child is reaped. */
static char **_argvFromArray(NSArray *strings, NSString *first) {
    NSUInteger count = [strings count];
    NSUInteger extra = (first != nil) ? 1 : 0;
    char **argv = calloc(count + extra + 1, sizeof(char *));

    if (argv == NULL) {
        return NULL;
    }
    if (first != nil) {
        argv[0] = strdup([first UTF8String]);
    }
    for (NSUInteger i = 0; i < count; i++) {
        argv[i + extra] = strdup([[strings objectAtIndex:i] UTF8String]);
    }
    return argv;
}

static char **_envpFromDictionary(NSDictionary *environment) {
    if (environment == nil) {
        return NULL;
    }

    NSArray *keys = [environment allKeys];
    NSUInteger count = [keys count];
    char **envp = calloc(count + 1, sizeof(char *));

    if (envp == NULL) {
        return NULL;
    }
    for (NSUInteger i = 0; i < count; i++) {
        NSString *key = [keys objectAtIndex:i];
        NSString *entry = [NSString stringWithFormat:@"%@=%@", key,
                                                     [environment objectForKey:key]];
        envp[i] = strdup([entry UTF8String]);
    }
    return envp;
}

static void _freeStringVector(char **vector) {
    if (vector == NULL) {
        return;
    }
    for (char **p = vector; *p != NULL; p++) {
        free(*p);
    }
    free(vector);
}

@implementation NSTask

+ (NSTask *)launchedTaskWithLaunchPath:(NSString *)path arguments:(NSArray *)arguments {
    NSTask *task = [[[self alloc] init] autorelease];

    [task setLaunchPath:path];
    [task setArguments:arguments];
    [task launch];
    return task;
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _processIdentifier = -1;
        _terminationStatus = 0;
        _terminationReason = NSTaskTerminationReasonExit;
    }
    return self;
}

- (void)dealloc {
    [_launchPath release];
    [_arguments release];
    [_environment release];
    [_currentDirectoryPath release];
    [_standardInput release];
    [_standardOutput release];
    [_standardError release];
    [super dealloc];
}

- (void)setLaunchPath:(NSString *)path {
    [path retain];
    [_launchPath release];
    _launchPath = path;
}

- (NSString *)launchPath {
    return _launchPath;
}

- (void)setArguments:(NSArray *)arguments {
    [arguments retain];
    [_arguments release];
    _arguments = arguments;
}

- (NSArray *)arguments {
    return _arguments;
}

- (void)setEnvironment:(NSDictionary *)environment {
    [environment retain];
    [_environment release];
    _environment = environment;
}

- (NSDictionary *)environment {
    return _environment;
}

- (void)setCurrentDirectoryPath:(NSString *)path {
    [path retain];
    [_currentDirectoryPath release];
    _currentDirectoryPath = path;
}

- (NSString *)currentDirectoryPath {
    return _currentDirectoryPath;
}

- (void)setStandardInput:(id)input {
    [input retain];
    [_standardInput release];
    _standardInput = input;
}

- (id)standardInput {
    return _standardInput;
}

- (void)setStandardOutput:(id)output {
    [output retain];
    [_standardOutput release];
    _standardOutput = output;
}

- (id)standardOutput {
    return _standardOutput;
}

- (void)setStandardError:(id)error {
    [error retain];
    [_standardError release];
    _standardError = error;
}

- (id)standardError {
    return _standardError;
}

- (void)launch {
    if (_launchPath == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"NSTask launched with no launch path"];
        return;
    }
    if (_isRunning) {
        [NSException raise:NSInvalidArgumentException
                    format:@"NSTask %@ is already running", _launchPath];
        return;
    }

    /* argv/envp are built before the fork: only async-signal-safe calls may
     * run in the child, and posix_spawn has no chdir action here. */
    char **argv = _argvFromArray(_arguments, _launchPath);
    char **envp = _envpFromDictionary(_environment);
    const char *launchPath = [_launchPath fileSystemRepresentation];
    const char *workingDirectory = (_currentDirectoryPath != nil)
        ? [_currentDirectoryPath fileSystemRepresentation] : NULL;
    int stdinFd = _descriptorForStream(_standardInput, NO);
    int stdoutFd = _descriptorForStream(_standardOutput, YES);
    int stderrFd = _descriptorForStream(_standardError, YES);

    pid_t pid = fork();

    if (pid == 0) {
        if (workingDirectory != NULL && chdir(workingDirectory) != 0) {
            _exit(127);
        }
        if (stdinFd >= 0)  { dup2(stdinFd,  STDIN_FILENO); }
        if (stdoutFd >= 0) { dup2(stdoutFd, STDOUT_FILENO); }
        if (stderrFd >= 0) { dup2(stderrFd, STDERR_FILENO); }

        execve(launchPath, argv, envp != NULL ? envp : environ);
        _exit(127);
    }

    _freeStringVector(argv);
    _freeStringVector(envp);

    if (pid < 0) {
        [NSException raise:NSInvalidArgumentException
                    format:@"NSTask could not fork for %@: %s", _launchPath, strerror(errno)];
        return;
    }

    _processIdentifier = (int)pid;
    _isRunning = YES;
    _hasTerminated = NO;

    /* The parent holds its own copies of the pipe ends; close ours so the
     * child sees EOF when it is the only writer left. */
    if ([_standardOutput isKindOfClass:[NSPipe class]]) {
        [[_standardOutput fileHandleForWriting] closeFile];
    }
    if ([_standardError isKindOfClass:[NSPipe class]] && _standardError != _standardOutput) {
        [[_standardError fileHandleForWriting] closeFile];
    }
    if ([_standardInput isKindOfClass:[NSPipe class]]) {
        [[_standardInput fileHandleForReading] closeFile];
    }
}

/* Reaps the child if it has exited. Returns YES once the exit status is known. */
- (BOOL)_reapWaiting:(BOOL)block {
    if (_hasTerminated) {
        return YES;
    }
    if (_processIdentifier < 0) {
        return NO;
    }

    int status = 0;
    pid_t result;

    do {
        result = waitpid((pid_t)_processIdentifier, &status, block ? 0 : WNOHANG);
    } while (result < 0 && errno == EINTR);

    if (result != (pid_t)_processIdentifier) {
        return NO;
    }

    if (WIFEXITED(status)) {
        _terminationStatus = WEXITSTATUS(status);
        _terminationReason = NSTaskTerminationReasonExit;
    } else if (WIFSIGNALED(status)) {
        _terminationStatus = WTERMSIG(status);
        _terminationReason = NSTaskTerminationReasonUncaughtSignal;
    }

    _isRunning = NO;
    _hasTerminated = YES;

    [[NSNotificationCenter defaultCenter] postNotificationName:NSTaskDidTerminateNotification
                                                        object:self];
    return YES;
}

- (void)interrupt {
    if (_processIdentifier > 0 && !_hasTerminated) {
        kill((pid_t)_processIdentifier, SIGINT);
    }
}

- (void)terminate {
    if (_processIdentifier > 0 && !_hasTerminated) {
        kill((pid_t)_processIdentifier, SIGTERM);
    }
}

- (BOOL)suspend {
    if (_processIdentifier > 0 && !_hasTerminated) {
        return kill((pid_t)_processIdentifier, SIGSTOP) == 0;
    }
    return NO;
}

- (BOOL)resume {
    if (_processIdentifier > 0 && !_hasTerminated) {
        return kill((pid_t)_processIdentifier, SIGCONT) == 0;
    }
    return NO;
}

- (int)processIdentifier {
    return _processIdentifier;
}

- (BOOL)isRunning {
    if (_isRunning) {
        [self _reapWaiting:NO];
    }
    return _isRunning;
}

- (int)terminationStatus {
    return _terminationStatus;
}

- (NSTaskTerminationReason)terminationReason {
    return _terminationReason;
}

- (void)waitUntilExit {
    [self _reapWaiting:YES];
}

@end
