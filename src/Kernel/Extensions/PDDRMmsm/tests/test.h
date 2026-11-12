// test.h - a minimal runner for the PDDRMmsm host tests
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#ifndef PD_MSM_TEST_H
#define PD_MSM_TEST_H

#include "fake.h"

extern int test_failures;
extern const char *test_current;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stdout, "  FAIL %s: %s:%d: %s\n", test_current, __FILE__, __LINE__, #cond); \
        test_failures++; \
    } \
} while (0)

#define CHECK_EQ(a, b) do { \
    unsigned long long _a = (unsigned long long)(a), _b = (unsigned long long)(b); \
    if (_a != _b) { \
        fprintf(stdout, "  FAIL %s: %s:%d: %s == %s (0x%llx != 0x%llx)\n", test_current, \
            __FILE__, __LINE__, #a, #b, _a, _b); \
        test_failures++; \
    } \
} while (0)

typedef void (*test_fn)(void);
struct test_case { const char *name; test_fn fn; };

#define TEST(name) static void name(void)

static inline int
run_tests(const char *suite, const struct test_case *tests, int n)
{
    int failed = 0;

    for (int i = 0; i < n; i++) {
        int before = test_failures;

        fake_reset();
        test_current = tests[i].name;
        tests[i].fn();
        if (test_failures != before)
            failed++;
        fprintf(stdout, "%s %s/%s\n", test_failures != before ? "FAIL" : "ok  ", suite, tests[i].name);
    }
    fprintf(stdout, "%s: %d of %d passed\n", suite, n - failed, n);
    return (failed != 0);
}

#define TEST_MAIN(suite, ...) \
    int test_failures; \
    const char *test_current; \
    int main(void) { \
        static const struct test_case t[] = { __VA_ARGS__ }; \
        return (run_tests(suite, t, (int)nitems(t))); \
    }

#define T(fn) { #fn, fn }

#endif
