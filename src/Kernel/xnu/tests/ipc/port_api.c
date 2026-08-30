#include <darwintest.h>
#include <darwintest_utils.h>

#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/mach_vm.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <mach/task.h>

#include <pthread.h>
#include <pthread/workqueue_private.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RUN_CONCURRENTLY(TRUE),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"));

T_DECL(mach_port_insert_right_123724977, "regression test for 123724977")
{
	mach_port_name_t pset;
	kern_return_t kr;

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &pset);
	T_ASSERT_MACH_SUCCESS(kr, "creating port set");

	kr = mach_port_insert_right(mach_task_self(), pset, pset,
	    MACH_MSG_TYPE_MAKE_SEND);
	T_ASSERT_MACH_ERROR(kr, KERN_INVALID_RIGHT, "insert right fails");
}

T_DECL(mach_port_name_rules, "make sure port names work correctly")
{
	mach_port_type_t ty;
	kern_return_t kr;
	mach_port_t mp, mp2;

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &mp);
	T_ASSERT_MACH_SUCCESS(kr, "creating port");
	T_ASSERT_EQ(mp & 0x3u, 0x3, "low bits are 0x3");

	kr = mach_port_type(mach_task_self(), mp, &ty);
	T_ASSERT_MACH_SUCCESS(kr, "mach_port_type");
	T_ASSERT_TRUE(ty & MACH_PORT_TYPE_RECEIVE, "mp is a receive right");

	kr = mach_port_type(mach_task_self(), mp & ~0x3u, &ty);
	T_ASSERT_MACH_ERROR(kr, KERN_INVALID_NAME,
	    "lookup is sensitive to the low bits");

	kr = mach_port_destruct(mach_task_self(), mp, 0, 0);
	T_ASSERT_MACH_SUCCESS(kr, "destroying port");

	kr = mach_port_type(mach_task_self(), mp, &ty);
	T_ASSERT_MACH_ERROR(kr, KERN_INVALID_NAME, "port is destroyed");

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &mp2);
	T_ASSERT_MACH_SUCCESS(kr, "creating port");
	T_ASSERT_EQ(mp2 & 0x3, 0x3, "low bits are 0x3");
	T_ASSERT_NE(mp, mp2, "port name will change");
	T_ASSERT_EQ(mp & ~0xffu, mp2 & ~0xffu,
	    "the index was reused with a generation delta of %d",
	    (mp2 - mp) >> 2);

	kr = mach_port_destruct(mach_task_self(), mp2, 0, 0);
	T_ASSERT_MACH_SUCCESS(kr, "destroying port");
}
