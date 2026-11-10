// notify(3) for Argon on Linux: names, state and dispatch handlers within one process for now
#pragma once
#include <stdint.h>
#include <dispatch/dispatch.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NOTIFY_STATUS_OK 0
#define NOTIFY_STATUS_INVALID_NAME 1
#define NOTIFY_STATUS_INVALID_TOKEN 2
#define NOTIFY_STATUS_FAILED 1000000
#ifdef __BLOCKS__
typedef void (^notify_handler_t)(int token);
#endif
uint32_t notify_post(const char *name);
uint32_t notify_register_check(const char *name, int *out_token);
#ifdef __BLOCKS__
uint32_t notify_register_dispatch(const char *name, int *out_token, dispatch_queue_t queue, notify_handler_t handler);
#endif
uint32_t notify_register_file_descriptor(const char *name, int *notify_fd, int flags, int *out_token);
uint32_t notify_check(int token, int *check);
uint32_t notify_get_state(int token, uint64_t *state);
uint32_t notify_set_state(int token, uint64_t state);
uint32_t notify_cancel(int token);
#ifdef __cplusplus
}
#endif
