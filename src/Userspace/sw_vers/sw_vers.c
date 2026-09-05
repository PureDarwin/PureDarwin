#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * SystemVersion.plist is the source of truth, as on Darwin. The constants
 * below are only a fallback for a system that has not staged one yet.
 */
#define SYSTEM_VERSION_PLIST "/System/Library/CoreServices/SystemVersion.plist"

#define PRODUCT_NAME "PureDarwin"
#define PRODUCT_VERSION "26.5"
#define BUILD_VERSION "25F74"

static char *plist_contents;

static void
load_plist(void)
{
    FILE *f = fopen(SYSTEM_VERSION_PLIST, "r");
    long size;

    if (f == NULL) {
        return;
    }
    if (fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) > 0 &&
        fseek(f, 0, SEEK_SET) == 0) {
        char *buf = malloc((size_t)size + 1);
        if (buf != NULL) {
            size_t got = fread(buf, 1, (size_t)size, f);
            buf[got] = '\0';
            plist_contents = buf;
        }
    }
    fclose(f);
}

/*
 * Minimal scan for <key>NAME</key> ... <string>VALUE</string>. sw_vers links
 * libSystem only, so there is no CFPropertyList here; the file is ours and
 * flat, and an unparseable one just falls back to the constants.
 */
static const char *
plist_value(const char *name)
{
    static char value[128];
    char key[128];
    const char *p, *start, *end;

    if (plist_contents == NULL) {
        return NULL;
    }
    if ((size_t)snprintf(key, sizeof(key), "<key>%s</key>", name) >= sizeof(key)) {
        return NULL;
    }
    if ((p = strstr(plist_contents, key)) == NULL) {
        return NULL;
    }
    if ((start = strstr(p, "<string>")) == NULL) {
        return NULL;
    }
    start += strlen("<string>");
    if ((end = strstr(start, "</string>")) == NULL) {
        return NULL;
    }
    if ((size_t)(end - start) >= sizeof(value)) {
        return NULL;
    }
    memcpy(value, start, (size_t)(end - start));
    value[end - start] = '\0';
    return value;
}

static const char *
version_field(const char *key, const char *fallback)
{
    const char *v = plist_value(key);

    return (v != NULL && v[0] != '\0') ? v : fallback;
}

static void
print_usage(void)
{
    fprintf(stderr, "usage: sw_vers [-productName|-productVersion|-buildVersion]\n");
}

int
main(int argc, char **argv)
{
    load_plist();

    if (argc == 1) {
        printf("ProductName:\t%s\n", version_field("ProductName", PRODUCT_NAME));
        printf("ProductVersion:\t%s\n", version_field("ProductVersion", PRODUCT_VERSION));
        printf("BuildVersion:\t%s\n", version_field("ProductBuildVersion", BUILD_VERSION));
        return 0;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "-productName") == 0) {
            printf("%s\n", version_field("ProductName", PRODUCT_NAME));
            return 0;
        }
        if (strcmp(argv[1], "-productVersion") == 0) {
            printf("%s\n", version_field("ProductVersion", PRODUCT_VERSION));
            return 0;
        }
        if (strcmp(argv[1], "-buildVersion") == 0) {
            printf("%s\n", version_field("ProductBuildVersion", BUILD_VERSION));
            return 0;
        }
    }

    print_usage();
    return 1;
}
