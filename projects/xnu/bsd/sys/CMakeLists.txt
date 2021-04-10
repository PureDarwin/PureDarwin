add_subdirectory(_types)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_sys DESTINATION user SUBDIRECTORY sys FILES
    appleapiopts.h	acct.h aio.h attr.h buf.h cdefs.h clonefile.h conf.h  dir.h dirent.h
    disk.h dkstat.h dtrace.h dtrace_glue.h dtrace_impl.h errno.h ev.h event.h fasttrap.h
    fasttrap_isa.h fcntl.h file.h filedesc.h filio.h fsgetpath.h gmon.h ioccom.h ioctl.h
    ioctl_compat.h ipc.h kernel.h kernel_types.h kern_event.h lctx.h loadable_fs.h lock.h
    lockf.h kauth.h kdebug.h kdebug_signpost.h kern_control.h lockstat.h malloc.h
    mbuf.h mman.h mount.h msg.h msgbuf.h netport.h param.h paths.h pipe.h poll.h
    proc.h  proc_info.h ptrace.h queue.h quota.h reboot.h resource.h resourcevar.h
    sbuf.h posix_sem.h posix_shm.h random.h sdt.h select.h sem.h semaphore.h shm.h signal.h
    signalvar.h snapshot.h socket.h socketvar.h sockio.h stat.h stdio.h sysctl.h syslimits.h
    syslog.h sys_domain.h termios.h time.h timeb.h times.h tprintf.h trace.h tty.h ttychars.h
    ttycom.h ttydefaults.h ttydev.h  types.h ubc.h ucontext.h ucred.h uio.h un.h unistd.h
    unpcb.h user.h utfconv.h utsname.h vadvise.h vcmd.h vm.h vmmeter.h vmparam.h vnioctl.h
    vnode.h vnode_if.h vsock.h vstat.h wait.h xattr.h _select.h _structs.h _types.h _endian.h
    domain.h protosw.h spawn.h timex.h commpage.h log_data.h

    ${CMAKE_CURRENT_BINARY_DIR}/sys/_posix_availability.h ${CMAKE_CURRENT_BINARY_DIR}/sys/_symbol_aliasing.h
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_sys DESTINATION user_private SUBDIRECTORY sys FILES
    attr.h cdefs.h clonefile.h coalition.h codesign.h content_protection.h csr.h decmpfs.h
    disk.h disklabel.h domain.h event.h fcntl.h fileport.h fsctl.h fsevents.h fsgetpath.h
    fslog.h guarded.h imgsrc.h ipcs.h kas_info.h kdebug.h kern_control.h kern_event.h
    kern_memorystatus.h kern_memorystatus_freeze.h kern_memorystatus_notify.h kern_overrides.h
    kern_sysctl.h mbuf.h mman.h monotonic.h persona.h priv.h proc.h proc_info.h proc_uuid_policy.h
    process_policy.h reason.h resource.h sfi.h shm_internal.h snapshot.h socket.h socketvar.h
    sockio.h spawn.h spawn_internal.h stackshot.h sys_domain.h tree.h ulock.h unpcb.h ux_exception.h
    vsock.h work_interval.h process_policy.h proc_uuid_policy.h priv.h pgo.h memory_maintenance.h
    commpage.h
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_sys DESTINATION kernel SUBDIRECTORY sys FILES
    appleapiopts.h attr.h buf.h cdefs.h conf.h dir.h dirent.h disk.h disklabel.h dkstat.h errno.h
    ev.h event.h fcntl.h file.h filio.h ioccom.h ioctl.h ipc.h ioctl_compat.h kernel.h kernel_types.h
    kern_event.h lock.h lockf.h kauth.h kdebug.h kdebug_kernel.h md5.h kern_control.h imgact.h malloc.h
    namei.h mman.h mbuf.h mount.h netport.h param.h paths.h proc.h queue.h random.h resource.h sbuf.h
    posix_sem.h posix_shm.h sem.h shm.h select.h signal.h socket.h socketvar.h sockio.h stat.h stdio.h
    sysctl.h syslimits.h syslog.h systm.h sys_domain.h time.h types.h ubc.h ucontext.h ucred.h uio.h
    un.h unistd.h unpcb.h utfconv.h unicode.h vm.h vmparam.h vnode.h vnode_if.h vsock.h xattr.h
    _select.h _structs.h _types.h _endian.h protosw.h domain.h kpi_mbuf.h kpi_socket.h kpi_socketfilter.h
    ttycom.h termios.h msg.h wait.h spawn.h timex.h commpage.h

    ${CMAKE_CURRENT_BINARY_DIR}/sys/_posix_availability.h ${CMAKE_CURRENT_BINARY_DIR}/sys/_symbol_aliasing.h
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_sys DESTINATION kernel_private SUBDIRECTORY sys FILES
    codesign.h cprotect.h content_protection.h csr.h decmpfs.h disktab.h eventhandler.h fbt.h
    fileport.h fsctl.h fslog.h kasl.h kern_memorystatus.h kern_memorystatus_freeze.h
    kern_memorystatus_notify.h kpi_private.h ktrace.h linker_set.h mach_swapon.h monotonic.h msgbuf.h
    eventvar.h persona.h proc_info.h pthread_shims.h quota.h reboot.h sem_internal.h shm_internal.h
    signalvar.h stackshot.h tty.h ttychars.h ttydefaults.h ttydev.h user.h vfs_context.h vmmeter.h
    reason.h spawn_internal.h priv.h pgo.h memory_maintenance.h doc_tombstone.h fsevents.h
    work_interval.h kern_sysctl.h vsock_transport.h

    ${CMAKE_CURRENT_BINARY_DIR}/sys/sysproto.h
)

set(makesyscalls ${CMAKE_CURRENT_SOURCE_DIR}/../kern/makesyscalls.sh)
set(syscalls_master ${CMAKE_CURRENT_SOURCE_DIR}/../kern/syscalls.master)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/trace.codes ${CMAKE_CURRENT_BINARY_DIR}/syscall.codes
    COMMAND sh -c ${makesyscalls} ${syscalls_master} trace > ${CMAKE_CURRENT_BINARY_DIR}/syscall.codes
    COMMAND sh -c sort -g ${CMAKE_CURRENT_BINARY_DIR}/../kern/trace_codes ${CMAKE_CURRENT_BINARY_DIR}/syscall.codes > ${CMAKE_CURRENT_BINARY_DIR}/trace.codes
    DEPENDS ${makesyscalls} ${syscalls_master}
    COMMENT "Generate trace.codes" VERBATIM
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sys/syscall.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${makesyscalls} ${syscalls_master} header > ${CMAKE_CURRENT_BINARY_DIR}/sys/syscall.h
    DEPENDS ${makesyscalls} ${syscalls_master}
    COMMENT "Generate syscall.h" VERBATIM
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sys/sysproto.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${makesyscalls} ${syscalls_master} proto
    DEPENDS ${makesyscalls} ${syscalls_master}
    COMMENT "Generate sysproto.h" VERBATIM
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sys/_posix_availability.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/make_posix_availability.sh ${CMAKE_CURRENT_BINARY_DIR}/_posix_availability.h
    DEPENDS make_posix_availability.sh
    COMMENT "Generate _posix_availability.h" VERBATIM
)

preprocess_file(make_symbol_aliasing.sh.in ${CMAKE_CURRENT_BINARY_DIR}/make_symbol_aliasing.sh
    AVAILABILITY_PL_PATH=$<TARGET_FILE:availability.pl>
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sys/_symbol_aliasing.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sys
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/make_symbol_aliasing.sh . ${CMAKE_CURRENT_BINARY_DIR}/sys/_symbol_aliasing.h
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/make_symbol_aliasing.sh
    COMMENT "Generate _symbol_aliasing.h" VERBATIM
)

add_darwin_object_library(xnu.bsd_sys_headers)
target_include_directories(xnu.bsd_sys_headers PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_sources(xnu.bsd_sys_headers PRIVATE ${XNU_SOURCE_DIR}/SETUP/empty.c)
