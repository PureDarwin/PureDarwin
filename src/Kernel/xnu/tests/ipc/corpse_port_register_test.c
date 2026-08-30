#include <darwintest.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

/*
 * This test verifies that we can:
 * 1. Generate a corpse port using task_generate_corpse
 * 2. Register the corpse port via the mach_ports_register API, ensuring it returns KERN_SUCCESS
 */
T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"),
	T_META_RUN_CONCURRENTLY(true));

static void
test_corpse_port_register(void)
{
	kern_return_t kr;
	mach_port_t corpse_port = MACH_PORT_NULL;

	T_LOG("Generating corpse port using task_generate_corpse");

	/* Generate a corpse port for the current task */
	kr = task_generate_corpse(mach_task_self(), &corpse_port);
	if (kr == KERN_RESOURCE_SHORTAGE) {
		T_SKIP("Corpse generation failed due to resource shortage - this is expected under load");
	}
	T_ASSERT_MACH_SUCCESS(kr, "task_generate_corpse should succeed");
	T_ASSERT_NE(corpse_port, MACH_PORT_NULL, "Should have received a valid corpse port");

	T_LOG("Generated corpse port: 0x%x", corpse_port);

	/* Now test registering the corpse port via mach_ports_register */
	mach_port_t ports_to_register[1] = {corpse_port};
	kr = mach_ports_register(mach_task_self(), ports_to_register, 1);
	T_ASSERT_MACH_SUCCESS(kr, "mach_ports_register should succeed with corpse port");

	T_LOG("Successfully registered corpse port via mach_ports_register");

	/* Clean up - unregister the ports */
	kr = mach_ports_register(mach_task_self(), NULL, 0);
	T_QUIET; T_EXPECT_MACH_SUCCESS(kr, "mach_ports_register cleanup");

	/* Clean up the corpse port */
	if (corpse_port != MACH_PORT_NULL) {
		kr = mach_port_deallocate(mach_task_self(), corpse_port);
		T_QUIET; T_EXPECT_MACH_SUCCESS(kr, "mach_port_deallocate corpse_port");
	}
}

T_DECL(corpse_port_register_test, "Test registering corpse port via mach_ports_register",
    T_META_IGNORECRASHES(".*sh.*"),
    T_META_CHECK_LEAKS(false))
{
	test_corpse_port_register();
}
