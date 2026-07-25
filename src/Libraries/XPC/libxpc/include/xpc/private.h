#ifndef XPC_PRIVATE_H_
#define XPC_PRIVATE_H_

#include <uuid/uuid.h>
#include <xpc/xpc.h>

#ifdef __cplusplus
extern "C" {
#endif

int _xpc_runtime_is_app_sandboxed();

typedef struct _xpc_pipe_s* xpc_pipe_t;

void xpc_pipe_invalidate(xpc_pipe_t pipe);

xpc_pipe_t xpc_pipe_create(int name, int arg2);

xpc_object_t _od_rpc_call(const char *procname, xpc_object_t payload, xpc_pipe_t (*get_pipe)(bool));

xpc_object_t xpc_create_with_format(const char * format, ...);

xpc_object_t xpc_create_from_plist(void *data, size_t size);

void xpc_dictionary_get_audit_token(xpc_object_t, audit_token_t *);
int xpc_pipe_routine_reply(xpc_object_t);
int xpc_pipe_routine(xpc_object_t pipe, void *payload,xpc_object_t *reply);

void xpc_connection_set_target_uid(xpc_connection_t connection, uid_t uid);
void xpc_connection_set_instance(xpc_connection_t connection, uuid_t uid);
void xpc_dictionary_set_mach_send(xpc_object_t object, const char* key, mach_port_t port);

// This must be reesonably unique, because it is tested against all
// XPC dictionaries sent to launchd, and we want to minimize the possibility
// of false matches. The other dictionary keys do not need to be as unique.
#define XPC_PROCESS_ROUTINE_KEY_OP "process-routine-operation"
#define XPC_PROCESS_ROUTINE_KEY_LABEL "label"
#define XPC_PROCESS_ROUTINE_KEY_ERROR "error"
#define XPC_PROCESS_ROUTINE_KEY_HANDLE "handle"
#define XPC_PROCESS_ROUTINE_KEY_NAME "name"
#define XPC_PROCESS_ROUTINE_KEY_PATH "path"
#define XPC_PROCESS_ROUTINE_KEY_ARGV "argv"
#define XPC_PROCESS_ROUTINE_KEY_TYPE "type"
#define XPC_PROCESS_ROUTINE_KEY_PID "pid"
#define XPC_PROCESS_ROUTINE_KEY_RCDATA "rcdata"
#define XPC_PROCESS_ROUTINE_KEY_SIGNAL "signal"
#define XPC_PROCESS_ROUTINE_KEY_PRIORITY_BAND "priority band"
#define XPC_PROCESS_ROUTINE_KEY_MEMORY_LIMIT "memory limit"

// Completely random. Not sure what the "actual" one is
#define XPC_PIPE_FLAG_PRIVILEGED 7

typedef enum {
	XPC_PROCESS_JETSAM_SET_BAND,
	XPC_PROCESS_JETSAM_SET_MEMORY_LIMIT,
	XPC_PROCESS_SERVICE_ATTACH,
	XPC_PROCESS_SERVICE_DETACH,
	XPC_PROCESS_SERVICE_GET_PROPERTIES,
	XPC_PROCESS_SERVICE_KILL
} xpc_process_command_t;


#ifdef __cplusplus
}
#endif

#endif

