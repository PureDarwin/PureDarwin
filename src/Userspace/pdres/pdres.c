// pdres.c - lists the main display's modes and switches between them, through the calls Settings uses
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void)
{
    fprintf(stderr, "usage: pdres [list]          the modes, * marks the current one\n"
                    "       pdres WIDTHxHEIGHT[@HZ] switch to that mode, the highest refresh when @HZ is left out\n"
                    "       pdres id N            switch to mode N as list numbers it\n");
}

static void describe(CGDisplayModeRef mode, char *out, size_t len)
{
    snprintf(out, len, "%zux%zu@%.0f", CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode),
             CGDisplayModeGetRefreshRate(mode));
}

static int list(CGDirectDisplayID display, CFArrayRef modes)
{
    CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);
    int32_t currentID = current != NULL ? CGDisplayModeGetIODisplayModeID(current) : -1;
    char text[48];

    for (CFIndex i = 0; i < CFArrayGetCount(modes); i++) {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);

        describe(mode, text, sizeof(text));
        printf("%c %3d  %s\n", CGDisplayModeGetIODisplayModeID(mode) == currentID ? '*' : ' ',
               (int)CGDisplayModeGetIODisplayModeID(mode), text);
    }
    if (current != NULL)
        CGDisplayModeRelease(current);
    return 0;
}

// the mode with this size and refresh, or with this size and the highest refresh when hz is 0
static CGDisplayModeRef find(CFArrayRef modes, size_t width, size_t height, double hz)
{
    CGDisplayModeRef best = NULL;

    for (CFIndex i = 0; i < CFArrayGetCount(modes); i++) {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
        double rate = CGDisplayModeGetRefreshRate(mode);

        if (CGDisplayModeGetWidth(mode) != width || CGDisplayModeGetHeight(mode) != height)
            continue;
        if (hz > 0 && (rate < hz - 0.5 || rate > hz + 0.5))
            continue;
        if (best == NULL || rate > CGDisplayModeGetRefreshRate(best))
            best = mode;
    }
    return best;
}

static CGDisplayModeRef findID(CFArrayRef modes, int32_t id)
{
    for (CFIndex i = 0; i < CFArrayGetCount(modes); i++) {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);

        if (CGDisplayModeGetIODisplayModeID(mode) == id)
            return mode;
    }
    return NULL;
}

// asks for the mode, then reports what the display settled on once the compositor has applied it
static int set(CGDirectDisplayID display, CGDisplayModeRef mode)
{
    char want[48], now[48] = "unknown";
    CGError error;

    describe(mode, want, sizeof(want));
    error = CGDisplaySetDisplayMode(display, mode, NULL);
    if (error != kCGErrorSuccess) {
        fprintf(stderr, "pdres: the compositor refused %s (error %d)\n", want, (int)error);
        return 1;
    }
    sleep(1);
    CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);
    if (current != NULL) {
        describe(current, now, sizeof(now));
        CGDisplayModeRelease(current);
    }
    printf("pdres: asked for %s, the display is now %s\n", want, now);
    return strcmp(want, now) == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    CGDirectDisplayID display = CGMainDisplayID();
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(display, NULL);
    int status;

    if (modes == NULL || CFArrayGetCount(modes) == 0) {
        fprintf(stderr, "pdres: the display lists no modes, is this inside the desktop session?\n");
        return 1;
    }
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "list") == 0)) {
        status = list(display, modes);
    } else if (argc == 3 && strcmp(argv[1], "id") == 0) {
        CGDisplayModeRef mode = findID(modes, (int32_t)strtol(argv[2], NULL, 10));

        if (mode == NULL) {
            fprintf(stderr, "pdres: no mode %s, see pdres list\n", argv[2]);
            status = 1;
        } else {
            status = set(display, mode);
        }
    } else if (argc == 2) {
        unsigned width = 0, height = 0;
        double hz = 0;

        if (sscanf(argv[1], "%ux%u@%lf", &width, &height, &hz) < 2) {
            usage();
            status = 2;
        } else {
            CGDisplayModeRef mode = find(modes, width, height, hz);

            if (mode == NULL) {
                fprintf(stderr, "pdres: the display has no %s mode, see pdres list\n", argv[1]);
                status = 1;
            } else {
                status = set(display, mode);
            }
        }
    } else {
        usage();
        status = 2;
    }
    CFRelease(modes);
    return status;
}
