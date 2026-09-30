#include <stdarg.h>
#include <stdio.h>
#include <mach/mach_time.h>
#include <unistd.h>

#include "airjitd.h"

static char note[120];

int pd_log_verbose;

static void vlog(const char *fmt, va_list ap) {
  char b[512];
  int n = vsnprintf(b, sizeof(b) - 2, fmt, ap);

  if (n < 0)
    return;

  if (n > (int)sizeof(b) - 2)
    n = sizeof(b) - 2;

  // fd 2: with pdstderr=1 the kernel logs it whatever it points at. The non-blocking
  // console dropped everything once its queue filled
  b[n++] = '\n';
  write(2, b, (size_t)n);
}

void logmsg(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vlog(fmt, ap);
  va_end(ap);
}

void logv(const char *fmt, ...) {
  va_list ap;

  if (!pd_log_verbose)
    return;

  va_start(ap, fmt);
  vlog(fmt, ap);
  va_end(ap);
}

void pd_note(const char *fmt, ...) {
  size_t used = strlen(note);
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(note + used, sizeof(note) - used, fmt, ap);
  va_end(ap);
}

void pd_note_clear(void) {
  note[0] = 0;
}

const char *pd_note_text(void) {
  return note;
}

uint64_t pd_now_us(void) {
  static mach_timebase_info_data_t tb;

  if (!tb.denom)
    mach_timebase_info(&tb);

  return mach_absolute_time() * tb.numer / tb.denom / 1000;
}
