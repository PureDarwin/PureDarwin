#include "control_port_movability_common.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RUN_CONCURRENTLY(TRUE),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"));

T_DECL(movable_control_ports_entitled, "Test getting movable task and thread ports and sending them between parent and child")
{
	test_movable_control_ports();
}
