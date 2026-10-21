#include <stdio.h>
#include <string.h>

#define PRODUCT_NAME "PureDarwin"
#define PRODUCT_VERSION "11.3"
#define BUILD_VERSION "20E241"

static void
print_usage(void)
{
    fprintf(stderr, "usage: sw_vers [-productName|-productVersion|-buildVersion]\n");
}

int
main(int argc, char **argv)
{
    if (argc == 1) {
        printf("ProductName:\t%s\n", PRODUCT_NAME);
        printf("ProductVersion:\t%s\n", PRODUCT_VERSION);
        printf("BuildVersion:\t%s\n", BUILD_VERSION);
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-productName") != 0 &&
            strcmp(argv[i], "-productVersion") != 0 &&
            strcmp(argv[i], "-buildVersion") != 0) {
            print_usage();
            return 1;
        }
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-productName") == 0) {
            printf("%s\n", PRODUCT_NAME);
        } else if (strcmp(argv[i], "-productVersion") == 0) {
            printf("%s\n", PRODUCT_VERSION);
        } else if (strcmp(argv[i], "-buildVersion") == 0) {
            printf("%s\n", BUILD_VERSION);
        }
    }

    return 0;
}
