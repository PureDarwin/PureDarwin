#ifndef CONTROL_PORT_MOVABILITY_COMMON_H
#define CONTROL_PORT_MOVABILITY_COMMON_H

#include <darwintest.h>
#include <darwintest_utils.h>
#include "ipc_utils.h"
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/message.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Get two send rights to the current task and thread port each */
void get_task_thread_control(mach_port_t *task_control, mach_port_t *thread_control, const char *destination);

/* Send task and thread control ports to destination */
void send_task_thread_control(mach_port_t destination, mach_port_t task_control, mach_port_t thread_control, const char *origin, const char *dest);

/* Receive task and thread control ports from connection */
void receive_task_thread_control(mach_port_t connection, const char *origin, const char *dest);

/* Fork a child task and test control port movability between parent and child */
pid_t fork_task(void);

/* Common test implementation for control port movability */
void test_movable_control_ports(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_PORT_MOVABILITY_COMMON_H */
