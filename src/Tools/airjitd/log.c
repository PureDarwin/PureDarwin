#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>

#include "airjitd.h"
#include "bridge.h"

static char note[120];

int pd_log_verbose;
int pd_watch_x = -1, pd_watch_y = -1;
// the bridge connection once it is open: lines go to the kext's IOLog, before that to fd 2
static io_connect_t log_conn;

void pd_log_attach(mach_port_t conn) {
  log_conn = conn;
}

// airjitdv on the kernel command line turns on the verbose lines, no plist edit needed
int pd_log_bootarg_verbose(void) {
  char args[1024];
  size_t len = sizeof(args) - 1;
  char *p;

  if (sysctlbyname("kern.bootargs", args, &len, NULL, 0) != 0)
    return 0;

  args[len] = 0;
  for (p = strstr(args, "airjitdv"); p; p = strstr(p + 1, "airjitdv")) {
    if ((p == args || p[-1] == ' ') && (p[8] == 0 || p[8] == ' ' || p[8] == '='))
      return 1;
  }

  return 0;
}

static void vlog(const char *fmt, va_list ap) {
  char b[512];
  int n = vsnprintf(b, sizeof(b) - 2, fmt, ap);

  if (n < 0)
    return;

  if (n > (int)sizeof(b) - 2)
    n = sizeof(b) - 2;

  if (log_conn && IOConnectCallStructMethod(log_conn, PD_BRIDGE_SEL_LOG, b, (size_t)n, NULL, NULL) == 0)
    return;

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

unsigned pd_cpus(void) {
  static unsigned n;

  if (!n) {
    long c = sysconf(_SC_NPROCESSORS_ONLN);

    n = c > 0 ? (unsigned)c : 1;
  }

  return n;
}

uint64_t pd_now_us(void) {
  static mach_timebase_info_data_t tb;

  if (!tb.denom)
    mach_timebase_info(&tb);

  return mach_absolute_time() * tb.numer / tb.denom / 1000;
}
