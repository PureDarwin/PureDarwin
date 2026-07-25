// TEST_CFLAGS -lobjc

#include "test.h"
#include <dlfcn.h>

// We use DYLD_LIBRARY_PATH to run the tests against a particular copy of
// libobjc. If this fails somehow (path is wrong, codesigning prevents loading,
// etc.) then the typical result is a silent failure and we end up testing
// /usr/lib/libobjc.A.dylib instead. This test detects when DYLD_LIBRARY_PATH is
// set but libobjc isn't loaded from it.
int main(int argc __unused, char **argv) {
    char *containingDirectory = realpath(dirname(argv[0]), NULL);
    testprintf("containingDirectory is %s\n", containingDirectory);

    char *dyldLibraryPath = getenv("DYLD_LIBRARY_PATH");
    testprintf("DYLD_LIBRARY_PATH is %s\n", dyldLibraryPath);

    if (dyldLibraryPath != NULL && strlen(dyldLibraryPath) > 0) {
        int foundMatch = 0;
        int foundNonMatch = 0;
        
        dyldLibraryPath = strdup(dyldLibraryPath);
        
        Dl_info info;
        int success = dladdr((void *)objc_msgSend, &info);
        testassert(success);

        testprintf("libobjc is located at %s\n", info.dli_fname);
        
        char *cursor = dyldLibraryPath;
        char *path;
        while ((path = strsep(&cursor, ":"))) {
            char *resolved = realpath(path, NULL);
            testprintf("Resolved %s to %s\n", path, resolved);
            if (strcmp(resolved, containingDirectory) == 0) {
                testprintf("This is equal to our containing directory, ignoring.\n");
                continue;
            }
            testprintf("Comparing %s and %s\n", resolved, info.dli_fname);
            int comparison = strncmp(resolved, info.dli_fname, strlen(resolved));
            free(resolved);
            if (comparison == 0) {
                testprintf("Found a match!\n");
                foundMatch = 1;
                break;
            } else {
                foundNonMatch = 1;
            }
        }

        testprintf("Finished searching, foundMatch=%d foundNonMatch=%d\n", foundMatch, foundNonMatch);
        testassert(foundMatch || !foundNonMatch);
    }
    succeed(__FILE__);
}
