#include "control_port_movability_common.h"

pid_t pid;

/* Get two send rights to the current task and thread port each */
void
get_task_thread_control(mach_port_t *task_control, mach_port_t *thread_control, const char *destination)
{
	// get our own movable task port
	kern_return_t kr = task_get_special_port(mach_task_self(), TASK_KERNEL_PORT, task_control);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] get task port for: %s", pid, destination);
	kr = task_get_special_port(mach_task_self(), TASK_KERNEL_PORT, task_control);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] get task port again for: %s", pid, destination);

	kr = thread_get_special_port(mach_thread_self(), THREAD_KERNEL_PORT, thread_control);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] get thread port for: %s", pid, destination);
	kr = thread_get_special_port(mach_thread_self(), THREAD_KERNEL_PORT, thread_control);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] get thread port again for: %s", pid, destination);
}

void
send_task_thread_control(mach_port_t destination, mach_port_t task_control, mach_port_t thread_control, const char *origin, const char *dest)
{
	// test copying and moving task control port
	kern_return_t kr = ipc_send_port(destination, task_control, MACH_MSG_TYPE_MOVE_SEND);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] move task port from %s to %s", pid, origin, dest);
	kr = ipc_send_port(destination, task_control, MACH_MSG_TYPE_COPY_SEND);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] copy parent task port from %s to %s", pid, origin, dest);
	ipc_deallocate_port(task_control);

	// test copying and moving thread control port
	kr = ipc_send_port(destination, thread_control, MACH_MSG_TYPE_COPY_SEND);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] copy thread port from %s to %s", pid, origin, dest);
	kr = ipc_send_port(destination, thread_control, MACH_MSG_TYPE_MOVE_SEND);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] move thread port from %s to %s", pid, origin, dest);
	ipc_deallocate_port(thread_control);
}

void
receive_task_thread_control(mach_port_t connection, const char *origin, const char *dest)
{
	mach_port_t port;
	kern_return_t kr = ipc_receive_port(connection, &port);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] %s received task port from %s", pid, dest, origin);
	T_ASSERT_NE(port, MACH_PORT_NULL, "[%d] task control port not null", pid);
	ipc_deallocate_port(port);

	kr = ipc_receive_port(connection, &port);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] %s received task port from %s", pid, dest, origin);
	T_ASSERT_NE(port, MACH_PORT_NULL, "[%d] task control port not null", pid);
	// Verify we can use the task port to get task info
	struct task_basic_info task_basic_info_data;
	mach_msg_type_number_t task_info_count = TASK_BASIC_INFO_COUNT;
	kr = task_info(port, TASK_BASIC_INFO,
	    (task_info_t)&task_basic_info_data, &task_info_count);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] task_info should succeed with %s task port", pid, origin);
	ipc_deallocate_port(port);

	kr = ipc_receive_port(connection, &port);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] %s received thread port from %s", pid, dest, origin);
	T_ASSERT_NE(port, MACH_PORT_NULL, "[%d] %s thread control port not null", pid, origin);
	// Verify we can use the thread port to get thread info
	struct thread_basic_info thread_basic_info_data;
	mach_msg_type_number_t thread_info_count = THREAD_BASIC_INFO_COUNT;
	kr = thread_info(port, THREAD_BASIC_INFO,
	    (thread_info_t)&thread_basic_info_data, &thread_info_count);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] thread_info should succeed with %s thread port", pid, origin);
	ipc_deallocate_port(port);

	kr = ipc_receive_port(connection, &port);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] %s received thread port from %s", pid, dest, origin);
	T_ASSERT_NE(port, MACH_PORT_NULL, "[%d] %s thread control port not null", pid, origin);
	ipc_deallocate_port(port);
}

pid_t
fork_task(void)
{
	kern_return_t kr;
	mach_port_t parent_bootstrap, child_bootstrap, child_connection;
	mach_port_t movable_task, movable_thread;

	// configure a new bootstrap port that will be inherited by our child
	// and can be used to send back the child's task and thread ports
	parent_bootstrap = ipc_create_receive_port();
	ipc_insert_send_right(parent_bootstrap);
	T_ASSERT_NE(parent_bootstrap, MACH_PORT_NULL, "[%d] create parent bootstrap port", pid);

	// create a second port for child to receive parent's task port
	child_bootstrap = ipc_create_receive_port();
	ipc_insert_send_right(child_bootstrap);
	T_ASSERT_NE(child_bootstrap, MACH_PORT_NULL, "[%d] create child bootstrap port", pid);

	kr = task_set_bootstrap_port(mach_task_self(), parent_bootstrap);
	T_ASSERT_MACH_SUCCESS(kr, "[%d] set bootstrap port", pid);

	pid = fork();
	T_ASSERT_POSIX_SUCCESS(pid, "[%d] fork", pid);

	if (0 != pid) {
		// parent
		// restore original bootstrap port
		kr = task_set_bootstrap_port(mach_task_self(), bootstrap_port);
		T_ASSERT_MACH_SUCCESS(kr, "[%d] restore bootstrap port", pid);

		// receive connection to child
		kr = ipc_receive_port(parent_bootstrap, &child_connection);
		T_ASSERT_MACH_SUCCESS(kr, "[%d] receive child connection port", pid);

		// get 2 send rights to our task and thread control ports each
		get_task_thread_control(&movable_task, &movable_thread, "parent");

		// send and copy our control ports to the child
		send_task_thread_control(child_connection, movable_task, movable_thread, "parent", "child");

		receive_task_thread_control(parent_bootstrap, "child", "parent");

		// Clean up bootstrap ports
		ipc_deallocate_port(parent_bootstrap);
		ipc_deallocate_port(child_bootstrap);
	} else {
		// child
		child_connection = ipc_create_receive_port();
		ipc_insert_send_right(child_connection);

		// get the bootstrap port inherited by the parent
		kr = task_get_bootstrap_port(mach_task_self(), &parent_bootstrap);
		T_ASSERT_MACH_SUCCESS(kr, "[%d] get bootstrap port in child", pid);

		// send the child bootstrap port for handshake
		kr = ipc_send_port(parent_bootstrap, child_connection, MACH_MSG_TYPE_MOVE_SEND);
		T_ASSERT_MACH_SUCCESS(kr, "[%d] send child connection port", pid);

		// get 2 send rights to our task and thread control ports each
		get_task_thread_control(&movable_task, &movable_thread, "child");

		// send and copy our control ports to the child
		send_task_thread_control(parent_bootstrap, movable_task, movable_thread, "child", "parent");

		receive_task_thread_control(child_connection, "parent", "child");

		// clean up ports
		ipc_deallocate_port(parent_bootstrap);

		exit(0);
	}

	return pid;
}

void
test_movable_control_ports(void)
{
	pid_t child_pid;
	int child_status;

	child_pid = fork_task();
	T_ASSERT_GT(child_pid, 0, "[%d] fork_task should succeed", pid);

	// Wait for child to exit
	int wait_result = waitpid(child_pid, &child_status, 0);
	T_ASSERT_EQ(wait_result, child_pid, "[%d] waitpid should return child pid", pid);
	T_EXPECT_TRUE(WIFEXITED(child_status), "[%d] child should exit normally", pid);
	T_EXPECT_EQ(WEXITSTATUS(child_status), 0, "[%d] child exit status should be 0", pid);
}
