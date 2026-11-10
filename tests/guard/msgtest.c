// mach messaging from a hardened runtime binary, the way gecko's child check-in does it
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

extern char **environ;

typedef struct {
	mach_msg_header_t header;
	mach_msg_body_t body;
	mach_msg_port_descriptor_t port;
	int value;
} pd_msg_t;

typedef struct {
	pd_msg_t msg;
	mach_msg_trailer_t trailer;
} pd_rcv_t;

static int child(const char *name)
{
	mach_port_t server = MACH_PORT_NULL, reply = MACH_PORT_NULL;
	kern_return_t kr = bootstrap_look_up(bootstrap_port, name, &server);
	if (kr != KERN_SUCCESS) {
		printf("child: bootstrap_look_up %s: %d\n", name, kr);
		return 1;
	}
	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply);

	pd_msg_t m;
	memset(&m, 0, sizeof(m));
	m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
	m.header.msgh_size = sizeof(m);
	m.header.msgh_remote_port = server;
	m.header.msgh_id = 200;
	m.body.msgh_descriptor_count = 1;
	m.port.name = reply;
	m.port.disposition = MACH_MSG_TYPE_MAKE_SEND;
	m.port.type = MACH_MSG_PORT_DESCRIPTOR;
	m.value = 42;
	kr = mach_msg(&m.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(m), 0, MACH_PORT_NULL, 5000, MACH_PORT_NULL);
	printf("child: send %d\n", kr);

	pd_rcv_t r;
	memset(&r, 0, sizeof(r));
	kr = mach_msg(&r.msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(r), reply, 5000, MACH_PORT_NULL);
	printf("child: reply %d value %d\n", kr, r.msg.value);
	return kr == KERN_SUCCESS && r.msg.value == 43 ? 0 : 1;
}

int main(int argc, char **argv)
{
	if (argc > 2 && strcmp(argv[1], "child") == 0)
		return child(argv[2]);

	// a kernel object call through MIG
	task_basic_info_data_t info;
	mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
	kern_return_t kr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count);
	printf("task_info %d\n", kr);

	char name[64];
	snprintf(name, sizeof(name), "org.puredarwin.msgtest.%d", getpid());
	mach_port_t server = MACH_PORT_NULL;
	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &server);
	mach_port_insert_right(mach_task_self(), server, server, MACH_MSG_TYPE_MAKE_SEND);
	kr = bootstrap_register(bootstrap_port, name, server);
	printf("bootstrap_register %d\n", kr);

	pid_t pid;
	char *args[] = { argv[0], "child", name, NULL };
	int err = posix_spawn(&pid, argv[0], NULL, NULL, args, environ);
	printf("posix_spawn %d\n", err);

	pd_rcv_t r;
	memset(&r, 0, sizeof(r));
	kr = mach_msg(&r.msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(r), server, 10000, MACH_PORT_NULL);
	printf("parent: check-in %d id %d value %d\n", kr, r.msg.header.msgh_id, r.msg.value);
	if (kr == KERN_SUCCESS) {
		pd_msg_t m;
		memset(&m, 0, sizeof(m));
		m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
		m.header.msgh_size = sizeof(m);
		m.header.msgh_remote_port = r.msg.port.name;
		m.header.msgh_id = 201;
		m.body.msgh_descriptor_count = 1;
		m.port.name = mach_task_self();
		m.port.disposition = MACH_MSG_TYPE_COPY_SEND;
		m.port.type = MACH_MSG_PORT_DESCRIPTOR;
		m.value = 43;
		kr = mach_msg(&m.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(m), 0, MACH_PORT_NULL, 5000, MACH_PORT_NULL);
		printf("parent: reply %d\n", kr);
	}

	int status = 0;
	waitpid(pid, &status, 0);
	printf("child status 0x%x\n", status);
	int ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
	printf("MSGTEST %s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
