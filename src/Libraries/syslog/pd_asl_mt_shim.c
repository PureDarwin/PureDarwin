/*
 * MessageTracer shim entry points for asl.c.
 *
 * Upstream asl_mt_shim.c forwards ASL messages to com.apple.analyticsd's
 * messagetracer service - Apple telemetry, entirely separate from logging - and
 * does so over xpc_pipe private SPI (XPC_PIPE_PRIVILEGED,
 * xpc_pipe_routine_with_flags) that PureDarwin's libxpc does not implement.
 * There is no analyticsd here to receive the messages either.
 *
 * Not forwarding telemetry loses nothing: asl.c calls these purely as a side
 * channel, after it has already delivered the message through the normal ASL
 * path.
 */

#include <asl_msg.h>

void _asl_mt_shim_fork_child(void);
void _asl_mt_shim_send_message(asl_msg_t *msg);

void
_asl_mt_shim_fork_child(void)
{
}

void
_asl_mt_shim_send_message(asl_msg_t *msg)
{
	(void)msg;
}
