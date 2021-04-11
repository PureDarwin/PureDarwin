install_xnu_headers(TARGET_NAME xnu.headers.osfmk_mach_i386 DESTINATION user SUBDIRECTORY mach/i386 FILES
    boolean.h exception.h fp_reg.h processor_info.h kern_return.h ndr_def.h syscall_sw.h
    thread_status.h thread_state.h vm_param.h vm_types.h rpc.h _structs.h sdt_isa.h
)
install_xnu_headers(TARGET_NAME xnu.headers.osfmk_mach_i386 DESTINATION kernel SUBDIRECTORY mach/i386 FILES
    boolean.h exception.h fp_reg.h processor_info.h kern_return.h ndr_def.h syscall_sw.h
    thread_status.h thread_state.h vm_param.h vm_types.h rpc.h _structs.h sdt_isa.h
)
install_xnu_headers(TARGET_NAME xnu.headers.osfmk_mach_i386 DESTINATION user_private SUBDIRECTORY mach/i386 FILES
    syscall_sw.h
)
