// hello-proc.c - /proc and /sys as a Linux program sees them under elftrans
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

static void check(int ok, const char *what) {
  printf("%s %s\n", ok ? "ok" : "FAIL", what);
  failures += !ok;
}

// the whole file, or NULL
static char *slurp(const char *path) {
  static char buf[1 << 20];
  int fd = open(path, O_RDONLY);
  ssize_t n, total = 0;

  if (fd < 0)
    return NULL;
  while ((n = read(fd, buf + total, sizeof(buf) - 1 - (size_t)total)) > 0)
    total += n;
  close(fd);
  buf[total] = 0;
  return buf;
}

int main(int argc, char **argv) {
  char link[PATH_MAX], *s;
  struct stat st;
  ssize_t n;
  long cpus = sysconf(_SC_NPROCESSORS_ONLN);

  (void)argc;
  n = readlink("/proc/self/exe", link, sizeof(link) - 1);
  link[n > 0 ? n : 0] = 0;
  check(n > 0 && strstr(link, "hello-proc"), "/proc/self/exe names this program");
  check(!stat("/proc/self/exe", &st) && S_ISREG(st.st_mode) && st.st_size > 0, "stat follows /proc/self/exe");
  s = slurp("/proc/self/cmdline");
  check(s && !strcmp(s, argv[0]), "/proc/self/cmdline starts with argv[0]");
  s = slurp("/proc/cpuinfo");
  check(s && strstr(s, "processor\t: 0") && strstr(s, "flags\t\t:") && strstr(s, " sse2"), "/proc/cpuinfo");
  s = slurp("/proc/meminfo");
  check(s && !strncmp(s, "MemTotal:", 9) && strstr(s, "MemAvailable:"), "/proc/meminfo");
  s = slurp("/proc/self/maps");
  check(s && strstr(s, "r-x") && strchr(s, '\n'), "/proc/self/maps");
  s = slurp("/proc/self/status");
  check(s && strstr(s, "Name:\thello-proc") && strstr(s, "Threads:"), "/proc/self/status");
  s = slurp("/proc/self/stat");
  check(s && atoi(s) == getpid() && strstr(s, "(hello-proc)"), "/proc/self/stat");
  s = slurp("/sys/devices/system/cpu/online");
  check(s && s[0] == '0', "/sys/devices/system/cpu/online");
  check(cpus >= 1, "sysconf(_SC_NPROCESSORS_ONLN)");
  {
    DIR *d = opendir("/proc/self/fd");
    struct dirent *e;
    int fd0 = 0, entries = 0;

    while (d && (e = readdir(d))) {
      entries++;
      fd0 |= !strcmp(e->d_name, "0");
    }
    if (d)
      closedir(d);
    check(fd0 && entries >= 3, "/proc/self/fd lists open fds");
  }
  n = readlink("/proc/self/fd/1", link, sizeof(link) - 1);
  check(n > 0, "/proc/self/fd/1 is a link");
  check(access("/proc/nope", F_OK) != 0, "a missing /proc file is missing");
  printf("cpus %ld\n%s\n", cpus, failures ? "proc tests FAILED" : "proc tests passed");
  return failures ? 1 : 0;
}
