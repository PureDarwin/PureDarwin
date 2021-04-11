install_xnu_headers(TARGET_NAME xnu.headers.osfmk_mach_machine DESTINATION user SUBDIRECTORY mach/machine FILES
    asm.h boolean.h exception.h kern_return.h ndr_def.h rpc.h
    processor_info.h thread_state.h thread_status.h
    vm_param.h vm_types.h machine_types.defs
    syscall_sw.h sdt.h sdt_isa.h _structs.h asm.h
)
install_xnu_headers(TARGET_NAME xnu.headers.osfmk_mach_machine DESTINATION kernel SUBDIRECTORY mach/machine FILES
    asm.h boolean.h exception.h kern_return.h ndr_def.h rpc.h
    processor_info.h thread_state.h thread_status.h
    vm_param.h vm_types.h machine_types.defs
    syscall_sw.h sdt.h sdt_isa.h _structs.h
)

install_xnu_headers(TARGET_NAME xnu.headers.osfmk_mach_machine DESTINATION user_private SUBDIRECTORY mach/machine FILES
    syscall_sw.h
)
