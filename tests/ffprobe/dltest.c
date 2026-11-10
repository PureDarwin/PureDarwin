#include <CoreVideo/CoreVideo.h>
#include <stdio.h>
#include <unistd.h>
static int n;
static CVReturn cb(CVDisplayLinkRef l, const CVTimeStamp *a, const CVTimeStamp *b, CVOptionFlags f, CVOptionFlags *o, void *c) { n++; return 0; }
int main(void) {
    CVDisplayLinkRef link = NULL;
    CVReturn r = CVDisplayLinkCreateWithActiveCGDisplays(&link);
    CVDisplayLinkSetOutputCallback(link, cb, NULL);
    CVDisplayLinkStart(link);
    sleep(2);
    printf("DLTEST create %d ticks %d\n", r, n);
    return 0;
}
