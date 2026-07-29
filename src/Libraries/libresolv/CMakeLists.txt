add_darwin_static_library(libresolv_static)
target_sources(libresolv_static PRIVATE
    base64.c
    dns.c
    dns_util.c
    ns_date.c
    ns_name.c
    ns_netint.c
    ns_parse.c
    ns_print.c
    ns_samedomain.c
    ns_ttl.c
    res_comp.c
    res_data.c
    res_debug.c
    res_init.c
    res_mkquery.c
    res_query.c
    res_send.c
    # dst_s_dns_key_id() etc, called by ns_print.c for DNSSEC KEY records.
    # Was a hand-written approximation in pd_net_compat.c; this is BIND's real
    # footprint calculation (RSA special case + checksum fallback).
    dst_support.c
    pd_res_compat.c
    pd_res_state.c
)

target_compile_definitions(libresolv_static PRIVATE
    __PUREDARWIN__
    # cdefs.h's generated libc-features.h #errors out unless this matches
    # what libc_static itself was compiled with (see libc/CMakeLists.txt).
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
)
# Same bridgeos-availability shim libc_static force-includes: private
# headers pulled in transitively (os/lock_private.h etc.) reference
# __API_AVAILABLE(...bridgeos(x)...), which neither the SDK nor PD define.
target_compile_options(libresolv_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>)

# netdb.h/math.h/arpa/nameser.h (copied in from the real Apple SDK - not
# vendored libresolv sources, just standard headers libresolv's .c files
# expect to find at those standard paths) live in libc's private include/
# tree, which headers.sh does NOT install into libc_public_headers' generated
# tree (that tree is libc-internal-only, e.g. os/assumes.h). Reach it
# directly rather than duplicating the headers here too.
target_include_directories(libresolv_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mDNSResponder/mDNSShared  # real dns_sd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include          # ifaddrs.h, notify.h stub
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin      # libc_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale       # xlocale_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale/FreeBSD # setlocale.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/stdtime/FreeBSD # timelocal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread   # <pthread.h> itself (quirky: lives inside a pthread/ dir)
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private            # <pthread/tsd_private.h>
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include   # <os/tsd.h>
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include          # <mach/vm_page_size.h>, <mach/clock.h>
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include                # <os/lock.h>
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private                # <os/lock_private.h>
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch                        # <dispatch/dispatch.h>
    # libc_features (linked below) only exposes its base generated include/
    # dir as INTERFACE; the actual generated libc-features.h lives one level
    # deeper, under a per-arch subdir that libc_static's own CMakeLists adds
    # to itself directly rather than through the libc_features interface.
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_link_libraries(libresolv_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)
add_dependencies(libresolv_static libc_features)
