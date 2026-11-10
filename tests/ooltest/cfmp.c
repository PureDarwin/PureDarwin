// cfmp.c - CFMessagePort requests large enough to travel out of line, with and without a reply
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

static CFDataRef callout(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info) {
    printf("CFMP got msgid %d length %ld\n", (int)msgid, (long)CFDataGetLength(data));
    fflush(stdout);
    if (msgid == 2) {
        UInt8 *big = calloc(1, 200000);
        CFDataRef reply = CFDataCreate(NULL, big, 200000);
        free(big);
        return reply;
    }
    return NULL;
}

int main(void) {
    char name[64];
    snprintf(name, sizeof(name), "org.puredarwin.cfmptest.%d", getpid());
    CFStringRef cfname = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    pid_t pid = fork();
    if (pid == 0) {
        sleep(1);
        CFMessagePortRef remote = CFMessagePortCreateRemote(NULL, cfname);
        UInt8 *big = calloc(1, 300000);
        CFDataRef request = CFDataCreate(NULL, big, 300000);
        CFDataRef reply = NULL;
        SInt32 r1 = CFMessagePortSendRequest(remote, 1, request, 5, 0, NULL, NULL);
        SInt32 r2 = CFMessagePortSendRequest(remote, 2, request, 5, 5, kCFRunLoopDefaultMode, &reply);
        printf("CFMP client oneway %d reply %d length %ld\n", (int)r1, (int)r2, reply ? (long)CFDataGetLength(reply) : -1L);
        return 0;
    }
    CFMessagePortRef local = CFMessagePortCreateLocal(NULL, cfname, callout, NULL, NULL);
    CFRunLoopSourceRef source = CFMessagePortCreateRunLoopSource(NULL, local, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 4, false);
    int status;
    waitpid(pid, &status, 0);
    printf("CFMPTEST done\n");
    return 0;
}
