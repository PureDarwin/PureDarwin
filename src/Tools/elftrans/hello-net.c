// hello-net.c - epoll, eventfd, timerfd, select and TCP/UDP over loopback from a Linux program under elftrans
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

static int failures;

static void check(int ok, const char *what) {
  printf("%s %s\n", ok ? "ok" : "FAIL", what);
  failures += !ok;
}

static double now(void) {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void *poke(void *arg) {
  uint64_t one = 1;

  usleep(50000);
  write(*(int *)arg, &one, 8);
  return 0;
}

int main(void) {
  int ep = epoll_create1(EPOLL_CLOEXEC), efd = eventfd(0, EFD_NONBLOCK), tfd;
  struct epoll_event ev = { 0 }, got[4];
  uint64_t value = 0;
  pthread_t t;
  double t0;

  check(ep >= 0 && efd >= 0, "epoll_create1 and eventfd");
  ev.events = EPOLLIN, ev.data.u64 = 0x1234;
  check(!epoll_ctl(ep, EPOLL_CTL_ADD, efd, &ev), "epoll_ctl add");
  check(epoll_wait(ep, got, 4, 0) == 0, "an empty eventfd is not ready");
  check(read(efd, &value, 8) < 0 && errno == EAGAIN, "reading it would block");
  pthread_create(&t, 0, poke, &efd);
  check(epoll_wait(ep, got, 4, 2000) == 1 && got[0].data.u64 == 0x1234 && (got[0].events & EPOLLIN),
        "epoll_wait wakes for an eventfd written from another thread");
  pthread_join(t, 0);
  check(read(efd, &value, 8) == 8 && value == 1 && epoll_wait(ep, got, 4, 0) == 0, "eventfd read resets it");

  tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  {
    struct itimerspec its = { { 0, 20000000 }, { 0, 30000000 } };

    ev.events = EPOLLIN, ev.data.u64 = 7;
    epoll_ctl(ep, EPOLL_CTL_ADD, tfd, &ev);
    t0 = now();
    check(!timerfd_settime(tfd, 0, &its, 0), "timerfd_settime 30 ms then every 20 ms");
    check(epoll_wait(ep, got, 4, 2000) == 1 && got[0].data.u64 == 7 && now() - t0 >= 0.025,
          "the timer fires after its first expiry");
    usleep(70000);
    check(read(tfd, &value, 8) == 8 && value >= 3, "and keeps firing at its interval");
    its.it_value.tv_nsec = 0, its.it_interval.tv_nsec = 0;
    timerfd_settime(tfd, 0, &its, 0);
  }

  {
    int lst = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0), cli, srv, one = 1, err = -1;
    struct sockaddr_in a = { 0 }, b = { 0 };
    socklen_t len = sizeof(a);
    char buf[16] = { 0 };
    fd_set rd;
    struct timeval tv = { 2, 0 };

    a.sin_family = AF_INET, a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    check(lst >= 0 && !setsockopt(lst, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)), "TCP socket, SO_REUSEADDR");
    check(!bind(lst, (struct sockaddr *)&a, sizeof(a)) && !listen(lst, 4) &&
              !getsockname(lst, (struct sockaddr *)&a, &len) && a.sin_family == AF_INET && a.sin_port,
          "bind, listen, getsockname on loopback");
    cli = socket(AF_INET, SOCK_STREAM, 0);
    check(!connect(cli, (struct sockaddr *)&a, sizeof(a)), "connect");
    len = sizeof(b);
    srv = accept4(lst, (struct sockaddr *)&b, &len, SOCK_NONBLOCK);
    check(srv >= 0 && b.sin_family == AF_INET && b.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "accept4 with the peer");
    check(!setsockopt(cli, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)), "TCP_NODELAY");
    send(cli, "ping", 4, MSG_NOSIGNAL);
    FD_ZERO(&rd);
    FD_SET(srv, &rd);
    check(select(srv + 1, &rd, 0, 0, &tv) == 1 && FD_ISSET(srv, &rd), "select sees the data");
    check(recv(srv, buf, sizeof(buf), 0) == 4 && !memcmp(buf, "ping", 4), "the bytes arrive");
    len = sizeof(err);
    check(!getsockopt(cli, SOL_SOCKET, SO_ERROR, &err, &len) && err == 0, "SO_ERROR is 0");
    close(srv);
    ev.events = EPOLLIN | EPOLLRDHUP, ev.data.u64 = 9;
    epoll_ctl(ep, EPOLL_CTL_ADD, cli, &ev);
    check(epoll_wait(ep, got, 4, 2000) == 1 && (got[0].events & EPOLLRDHUP), "the peer's close is EPOLLRDHUP");
    close(cli), close(lst);
  }

  {
    int u1 = socket(AF_INET, SOCK_DGRAM, 0), u2 = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { 0 }, from = { 0 };
    socklen_t len = sizeof(a);
    char buf[8] = { 0 };

    a.sin_family = AF_INET, a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(u1, (struct sockaddr *)&a, sizeof(a));
    getsockname(u1, (struct sockaddr *)&a, &len);
    check(sendto(u2, "dgram", 5, 0, (struct sockaddr *)&a, sizeof(a)) == 5, "UDP sendto");
    len = sizeof(from);
    check(recvfrom(u1, buf, sizeof(buf), 0, (struct sockaddr *)&from, &len) == 5 && from.sin_family == AF_INET &&
              len == sizeof(from),
          "UDP recvfrom with the sender's address");
    close(u1), close(u2);
  }

  {
    int s6 = socket(AF_INET6, SOCK_STREAM, 0), one = 1;
    struct sockaddr_in6 a = { 0 };
    socklen_t len = sizeof(a);

    a.sin6_family = AF_INET6, a.sin6_addr = in6addr_loopback;
    check(s6 >= 0 && !setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one)) &&
              !bind(s6, (struct sockaddr *)&a, sizeof(a)) && !getsockname(s6, (struct sockaddr *)&a, &len) &&
              a.sin6_family == AF_INET6 && a.sin6_port,
          "IPv6 socket, IPV6_V6ONLY, bind on ::1");
    close(s6);
  }
  printf("%s\n", failures ? "net tests FAILED" : "net tests passed");
  return failures ? 1 : 0;
}
