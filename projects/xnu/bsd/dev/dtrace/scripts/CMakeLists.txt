install(FILES darwin.d DESTINATION usr/lib/dtrace COMPONENT BaseSystem)
install(FILES errno.d DESTINATION usr/lib/dtrace COMPONENT BaseSystem)
install(FILES io.d DESTINATION usr/lib/dtrace COMPONENT BaseSystem)
install(FILES ip.d DESTINATION usr/lib/dtrace COMPONENT BaseSystem)
install(FILES sched.d DESTINATION usr/lib/dtrace COMPONENT BaseSystem)
install(FILES signal.d DESTINATION usr/lib/dtrace COMPONENT BaseSystem)
install(FILES socket.d DESTINATION usr/lib/dtrace COMPONENT BaseSystem)
install(FILES tcp.d DESTINATION usr/lib/dtrace COMPONENT BaseSystem)
install(FILES log_unnest_badness.d DESTINATION usr/libexec COMPONENT BaseSystem)
install(FILES vm_map_delete_permanent.d DESTINATION usr/libexec COMPONENT BaseSystem)
install(FILES vm_object_ownership.d DESTINATION usr/libexec COMPONENT BaseSystem)

# NOTE: If building ARM64, install regs_arm64.d and ptrauth_arm64.d to usr/lib/dtrace/arm64.
#       If building ARM, install regs_arm.d to usr/lib/dtrace/arm.
install(FILES regs_x86_64.d vmx_compat.h DESTINATION usr/lib/drace/x86_64 COMPONENT BaseSystem)
