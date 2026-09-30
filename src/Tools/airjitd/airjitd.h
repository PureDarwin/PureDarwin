#ifndef AIRJITD_H
#define AIRJITD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PD_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

// daemon error codes, reported to the kext as "daemon error N"
enum {
  PD_ERR_NO_BITCODE = -1,
  PD_ERR_BAD_NAME = -2,
  PD_ERR_PARSE = -3,
  PD_ERR_JIT_CREATE = -4,
  PD_ERR_JIT_ADD = -5,
  PD_ERR_JIT_LOOKUP = -6,
  PD_ERR_NO_ENTRY = -7,
  PD_ERR_TOO_MANY_PARAMS = -8,
  PD_ERR_TOO_MANY_RESULTS = -9,
  PD_ERR_BAD_FUNCTION = -10,
  PD_ERR_BAD_DATA = -11,
  PD_ERR_BAD_STAGE = -12,
  PD_ERR_STAGE_DISABLED = -13,
  PD_ERR_BAD_JOB = -14,
  PD_ERR_BAD_REQUEST = -20,
  PD_ERR_NOT_COMPILED = -21,
  PD_ERR_OUT_OF_RANGE = -22,
  PD_ERR_SHORT_VERTEX = -23,
  PD_ERR_SHORT_FRAGMENT = -24,
  PD_ERR_BUFFER = -25,
  PD_ERR_FRAGMENT_BUFFER = -26,
  PD_ERR_TEXTURE = -27,
  PD_ERR_FAULT = -40,
};

void logmsg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// per-job and per-draw detail, only with -v: every line reaches a serial console
extern int pd_log_verbose;
void pd_job_blame(uint32_t id);
void logv(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// monotonic microseconds, for timing jobs
uint64_t pd_now_us(void);

// the console is often unwritable (getty holds it): failures also travel back in the bridge
void pd_note(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void pd_note_clear(void);
const char *pd_note_text(void);

// ends the current job from inside a fault or a fatal backend error
void pd_job_abort(int sig, const char *why);

// runs fn(arg) so a fault on this thread unwinds back here: returns the signal, 0 when none
int pd_guarded(void (*fn)(void *), void *arg);

#endif
