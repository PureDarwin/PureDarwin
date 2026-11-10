// hello-process.c - fork, exec, wait and posix_spawn from a Linux program under elftrans
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int failures;

static void check(int ok, const char *what) {
  printf("%s %s\n", ok ? "ok" : "FAIL", what);
  failures += !ok;
}

// the exec'd copy of this program: what it was asked to look at, as its exit status
static int child(const char *mode, const char *argv0) {
  if (!strcmp(mode, "exec"))
    return strcmp(argv0, "renamed") ? 10 : 3;
  if (!strcmp(mode, "ignored")) {
    raise(SIGUSR1);
    return 4;
  }
  if (!strcmp(mode, "masked")) {
    sigset_t s;

    sigprocmask(SIG_BLOCK, 0, &s);
    return sigismember(&s, SIGUSR2) ? 5 : 11;
  }
  return 12;
}

static int run_self(const char *self, const char *mode, const char *argv0) {
  pid_t pid = fork();
  int status = -1;

  if (!pid) {
    char *args[] = { (char *)argv0, (char *)mode, 0 };

    execve(self, args, environ);
    _exit(99);
  }
  waitpid(pid, &status, 0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int main(int argc, char **argv) {
  const char *self = getenv("HELLO_PROCESS_SELF");
  int status = -1;
  pid_t pid;

  if (argc > 1)
    return child(argv[1], argv[0]);
  if (!self)
    self = argv[0];

  pid = fork();
  if (!pid)
    _exit(7);
  check(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 7, "fork, exit 7, wait4");

  pid = fork();
  if (!pid) {
    kill(getpid(), SIGTERM);
    pause();
    _exit(0);
  }
  check(waitpid(pid, &status, 0) == pid && WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM, "killed by SIGTERM");

  pid = fork();
  if (!pid)
    _exit(9);
  {
    siginfo_t si;

    memset(&si, 0, sizeof(si));
    check(!waitid(P_PID, (id_t)pid, &si, WEXITED) && si.si_pid == pid && si.si_code == CLD_EXITED && si.si_status == 9,
          "waitid");
  }

  check(run_self(self, "exec", "renamed") == 3, "exec of itself through elftrans, argv[0] kept");

  signal(SIGUSR1, SIG_IGN);
  check(run_self(self, "ignored", "x") == 4, "an ignored signal stays ignored across exec");
  signal(SIGUSR1, SIG_DFL);

  {
    sigset_t s, old;

    sigemptyset(&s);
    sigaddset(&s, SIGUSR2);
    sigprocmask(SIG_BLOCK, &s, &old);
    check(run_self(self, "masked", "x") == 5, "the signal mask carries across exec");
    sigprocmask(SIG_SETMASK, &old, 0);
  }

  {
    char *args[] = { "/bin/echo", "spawned a native program", 0 };

    status = -1;
    check(!posix_spawn(&pid, "/bin/echo", 0, 0, args, environ) && waitpid(pid, &status, 0) == pid &&
              WIFEXITED(status) && !WEXITSTATUS(status),
          "posix_spawn of a native program");
  }
  printf("%s\n", failures ? "process tests FAILED" : "process tests passed");
  return failures ? 1 : 0;
}
