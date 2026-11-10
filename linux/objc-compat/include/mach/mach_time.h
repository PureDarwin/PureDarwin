// mach_absolute_time for Argon on Linux, nanoseconds of CLOCK_MONOTONIC with a 1/1 timebase
#pragma once
#include <stdint.h>
#include <time.h>
#include <mach/mach_types.h>
struct mach_timebase_info { uint32_t numer; uint32_t denom; };
typedef struct mach_timebase_info *mach_timebase_info_t;
typedef struct mach_timebase_info mach_timebase_info_data_t;
static inline kern_return_t mach_timebase_info(mach_timebase_info_t info) { info->numer = 1; info->denom = 1; return KERN_SUCCESS; }
static inline uint64_t mach_absolute_time(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}

// sleeps until a mach_absolute_time deadline
static inline kern_return_t mach_wait_until(uint64_t deadline)
{
    struct timespec when = { (time_t)(deadline / 1000000000ull), (long)(deadline % 1000000000ull) };
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &when, NULL) != 0) {}
    return KERN_SUCCESS;
}
