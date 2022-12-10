
#if __has_feature(ptrauth_calls)
#include <ptrauth.h>
#endif

typedef unsigned long long uint64_t;
extern int printf(const char*, ...);
extern void exit(int);

#if __cplusplus
extern "C" {
#endif  /* __cplusplus */
__attribute__((format(printf, 3, 4)))
__attribute__ ((noreturn))
extern void _PASS(const char* file, unsigned line, const char* format, ...);

__attribute__((format(printf, 3, 4)))
__attribute__ ((noreturn))
extern void _FAIL(const char* file, unsigned line, const char* format, ...);

__attribute__((format(printf, 3, 4)))
extern void _LOG(const char* file, unsigned line, const char* format, ...);

extern void _TIMEOUT(const char* file, unsigned line, uint64_t seconds);
#if __cplusplus
};
#endif  /* __cplusplus */

// The arm64e kernel/userland ABIs sign C function pointers differently.
// We aren't trying to test that ABI, so lets just force a different signature
#if __has_feature(ptrauth_calls)
#   define PtrAuth __ptrauth(ptrauth_key_function_pointer, 1, 0xc671)
#else
#   define PtrAuth
#endif

typedef struct TestRunnerFunctions {
    uint64_t                            version;
    const struct mach_header*           mhs[4];
    const void*                         basePointers[4];
    PtrAuth __typeof(&printf)           printf;
    PtrAuth __typeof(&exit)             exit;
    PtrAuth __typeof(&_PASS)            testPass;
    PtrAuth __typeof(&_FAIL)            testFail;
    PtrAuth __typeof(&_LOG)             testLog;
    PtrAuth __typeof(&_TIMEOUT)         testTimeout;
} TestRunnerFunctions;

static const TestRunnerFunctions* funcs = 0;

#define PASS(...)           funcs->testPass(__FILE__,__LINE__,__VA_ARGS__)
#define FAIL(...)           funcs->testFail(__FILE__,__LINE__,__VA_ARGS__)
#define LOG(...)            funcs->testLog(__FILE__,__LINE__,__VA_ARGS__)
#define TIMEOUT(seconds)    funcs->testTimeout(__FILE__,__LINE__,seconds)

#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
static inline void setFuncs(const TestRunnerFunctions* v) {
    funcs = v;
}
