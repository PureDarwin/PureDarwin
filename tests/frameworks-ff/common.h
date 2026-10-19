// tiny check helpers shared by the framework tests
#include <stdio.h>
#include <stdlib.h>
static int failures;
#define CHECK(cond, ...) do { if (cond) printf("ok   " __VA_ARGS__); else { printf("FAIL " __VA_ARGS__); failures++; } printf("\n"); } while (0)
#define DONE(name) do { printf("%s: %s (%d failures)\n", name, failures ? "FAILED" : "PASSED", failures); return failures != 0; } while (0)
