add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/include/libc-features.h
    COMMAND ${CMAKE_COMMAND} -E env "ARCHS=x86_64 arm64" SRCROOT=${CMAKE_CURRENT_SOURCE_DIR} DERIVED_FILES_DIR=${CMAKE_CURRENT_BINARY_DIR}/include VARIANT_PLATFORM_NAME=macosx perl ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_features.pl
    COMMENT "Generating libc-features.h"
)

# The perl script won't do this for us
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)

add_darwin_object_library(libc_features)

target_include_directories(libc_features INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include)

target_sources(libc_features PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/include/libc-features.h
)

# We install the standard C headers to a skeleton tree here
set(libc_headers_dir ${CMAKE_CURRENT_BINARY_DIR}/headers)

add_custom_command(
    OUTPUT ${libc_headers_dir}/libc_headers.stamp
    COMMAND ${CMAKE_COMMAND} -E env "ARCHS=arm64 x86_64"
        SRCROOT=${CMAKE_CURRENT_SOURCE_DIR}
        DERIVED_FILES_DIR=${CMAKE_CURRENT_BINARY_DIR}
        VARIANT_PLATFORM_NAME=macosx
        DEPLOYMENT_LOCATION=NO
        BUILT_PRODUCTS_DIR=${libc_headers_dir}
        SDK_INSTALL_HEADERS_ROOT=""
        sh -c "${CMAKE_CURRENT_SOURCE_DIR}/scripts/headers.sh"
    COMMAND ${CMAKE_COMMAND} -E touch ${libc_headers_dir}/libc_headers.stamp
    COMMENT "Copying libC headers..."
)
add_custom_target(libc_headers_gen_script DEPENDS ${libc_headers_dir}/libc_headers.stamp)

add_library(libc_headers_gen INTERFACE)
add_library(libc_headers_gen_private INTERFACE)

target_include_directories(libc_headers_gen INTERFACE ${libc_headers_dir}/usr/include)
add_dependencies(libc_headers_gen INTERFACE libc_headers_gen_script)

target_include_directories(libc_headers_gen_private INTERFACE
    ${libc_headers_dir}/usr/include
    ${libc_headers_dir}/usr/local/include
    ${libc_headers_dir}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders
    ${libc_headers_dir}/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders
)

# This is the only public header directory
install(DIRECTORY ${libc_headers_dir}/usr/include DESTINATION usr COMPONENT DeveloperTools)

add_subdirectory(libm/openlibm)

# pd-compat-include holds headers we hand-wrote (real Apple's makeheaders.pl
# script above never generates them, since they don't exist upstream) that
# other consumers besides libc itself need too - CoreFoundation's
# CFBundle_Grok.c/CFBundle_Binary.c include <mach-o/dyld_priv.h> for
# dyld_image_path_containing_address. Install just that subset publicly
# rather than the whole pd-compat-include tree, which also holds headers
# meant to stay libc-internal (e.g. the reconstructed apfs_fsctl.h).
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pd-compat-include/mach-o/dyld_priv.h
        DESTINATION usr/include/mach-o COMPONENT DeveloperTools)

# These are private
install(DIRECTORY ${libc_headers_dir}/usr/local/include DESTINATION usr/local COMPONENT DeveloperInternal)
install(DIRECTORY ${libc_headers_dir}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders COMPONENT DeveloperInternal)
install(DIRECTORY ${libc_headers_dir}/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders DESTINATION System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders COMPONENT DeveloperInternal)

# END GENERATE INCLUDE DIRECTORY

add_library(libc_private_headers INTERFACE)
add_library(libc_kernel_headers INTERFACE)
add_library(libc_public_headers INTERFACE)

# stdlib.h includes malloc/_malloc.h
target_link_libraries(libc_public_headers INTERFACE libmalloc_headers)

# private headers implies public headers
target_link_libraries(libc_private_headers INTERFACE libc_public_headers)

# Make sure we install headers before use
target_link_libraries(libc_public_headers INTERFACE libc_headers_gen)
target_link_libraries(libc_kernel_headers INTERFACE libc_headers_gen)

target_include_directories(libc_public_headers INTERFACE ${libc_headers_dir}/usr/include)
target_include_directories(libc_public_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/wrappers)

target_include_directories(libc_private_headers INTERFACE ${libc_headers_dir}/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders)
target_include_directories(libc_private_headers INTERFACE ${libc_headers_dir}/usr/local/include)

target_include_directories(libc_kernel_headers INTERFACE ${libc_headers_dir}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders)

# END HEADER INTERFACE

add_darwin_static_library(libc_static)
target_sources(libc_static PRIVATE
    string/strcat.c
    string/strncat.c
    string/pd_strnlen.c          # _strnlen / _strlcpy
    string/FreeBSD/strdup.c
    string/FreeBSD/strtok.c      # _strtok (init /etc/ttys tokenizing)
    string/pd_strtok_r.c         # _strtok_r forwarder (__weak_reference alias not emitted)
    stdlib/FreeBSD/setenv.c      # _setenv/_unsetenv (init sets HOME/PATH/etc)
    gen/FreeBSD/waitpid.c        # _waitpid (init reaps/respawns children)
    gen/FreeBSD/wait.c           # __wait
    gen/FreeBSD/wait3.c          # _wait3 (zsh job/process accounting)
    gen/oldsyslog.c              # openlog/closelog/syslog (curl)
    gen/FreeBSD/wordexp.c        # wordexp/wordfree (fastfetch shell-style expansion)
    gen/NetBSD/utmpx.c           # setutxent/getutxent/endutxent (fastfetch users detection)
    gen/utmpx-darwin.c          # __utx_magic__/_utmpx32_64 (utmpx.c internals)
    sys/posix_spawn.c           # posix_spawnp (fastfetch/popen/wordexp)
    sys/fork.c                  # fork()
    sys/_libc_init.c            # _libc_initializer/_libc_fork_init (wires fork()'s atfork hooks)
    sys/_libc_fork_child.c      # _libc_fork_child (environ/arc4/clock-port fixup on the child side of fork)
    gen/FreeBSD/utime.c         # git
    gen/pd_wait.c                # _wait forwarder (__weak_reference alias not emitted)
    compat-43/FreeBSD/killpg.c   # _killpg (zsh job control)
    gen/FreeBSD/nice.c           # _nice (zsh BGNICE backgrounded-job priority)
    gen/FreeBSD/popen.c          # popen/pclose
    gen/FreeBSD/pause.c          # _pause (init halt loop)
    string/FreeBSD/strrchr.c
    string/FreeBSD/strsep.c      # _strsep (exec.c PATH split)
    string/stpcpy.c              # _stpcpy
    string/stpncpy.c             # _stpncpy
    secure/stpcpy_chk.c          # ___stpcpy_chk
    secure/stpncpy_chk.c         # ___stpncpy_chk
    stdlib/FreeBSD/div.c         # _div (ICU)
    stdlib/FreeBSD/random.c      # real random(3) family (replaces xorshift stubs)
    stdlib/FreeBSD/reallocf.c    # _reallocf (ld64 self-host)
    gen/FreeBSD/mkpath_np.c      # _mkpath_np/mkpath_np (ld64 self-host; real Apple Libc source, was only excluded in cctools' copy assuming real macOS already has it)
    gen/FreeBSD/getmntinfo.c     # real getmntinfo over getfsstat (df)
    stdio/FreeBSD/tmpfile.c      # _tmpfile (i3/pango)
    stdio/FreeBSD/open_memstream.c # _open_memstream (zsh)
    gen/fts.c                    # _fts_*$INODE64 (nftw dependency)
    gen/nftw.c                   # _nftw$INODE64 (glib/i3)
    gen/FreeBSD/glob.c           # _glob$INODE64 / _globfree
    gen/FreeBSD/siglist.c        # _sys_siglist / _sys_signame
    gen/confstr.c                # _confstr (shell_cmds getopt et al)
    secure/strlcat_chk.c         # ___strlcat_chk
    # wide-char stdio (text_cmds/shell_cmds: tr/cut/fold/echo/... all do
    # putwchar/getwchar-based I/O). vfwprintf.c brings wprintf/fwprintf.
    stdio/FreeBSD/putwchar.c
    stdio/FreeBSD/putwc.c
    stdio/FreeBSD/fputwc.c
    stdio/FreeBSD/fputws.c
    stdio/FreeBSD/getwchar.c
    stdio/FreeBSD/getwc.c
    stdio/FreeBSD/fgetwc.c
    stdio/FreeBSD/fgetws.c
    stdio/FreeBSD/fgetwln.c
    stdio/FreeBSD/ungetwc.c
    stdio/FreeBSD/fwide.c
    stdio/FreeBSD/wprintf.c
    gen/get_compat.c             # _compat_mode (uname/ps COMMAND_MODE checks)
    gen/FreeBSD/setmode.c        # _setmode/_getmode (uudecode -m, chmod-style modes)
    string/FreeBSD/wcscoll.c     # _wcscoll (comm)
    string/FreeBSD/wcschr.c      # _wcschr
    string/FreeBSD/wcsdup.c      # _wcsdup
    string/FreeBSD/wmemmove.c    # _wmemmove (fold)
    # wide-char + xlocale surface pulled in by the full libc++.dylib
    # (locale.cpp / wide std::sto* / collate). See libcxx-dylib.nix.
    string/FreeBSD/wmemset.c     # _wmemset
    string/FreeBSD/strxfrm.c     # _strxfrm / _strxfrm_l
    string/FreeBSD/wcsxfrm.c     # _wcsxfrm / _wcsxfrm_l
    locale/FreeBSD/wcstoll.c     # _wcstoll
    locale/FreeBSD/wcstoull.c    # _wcstoull
    locale/FreeBSD/wcstoul.c     # _wcstoul
    locale/FreeBSD/wcstod.c      # _wcstod
    locale/FreeBSD/wcstof.c      # _wcstof
    locale/FreeBSD/wcstold.c     # _wcstold
    stdio/FreeBSD/vswprintf.c    # _vswprintf_l (swprintf's engine)
    stdio/FreeBSD/swprintf.c     # _swprintf
    locale/FreeBSD/mbsrtowcs.c   # _mbsrtowcs
    locale/FreeBSD/nextwctype.c  # _nextwctype (tr [:class:] iteration)
    stdlib/FreeBSD/merge.c       # _mergesort (tr)
    nls/FreeBSD/msgcat.c         # _catopen/_catgets/_catclose (uniq NLS)
    stdio/FreeBSD/freopen.c      # _freopen (expand/fold/uuencode stdin rebind)
    # BSD libc db (dbopen w/ hash+btree+recno backends): tsort uses DB_HASH;
    # future pwd_mkdb/dev_mkdb/cap_mkdb consumers use hash too.
    db/db/FreeBSD/db.c
    db/hash/FreeBSD/hash.c
    db/hash/FreeBSD/hash_bigkey.c
    db/hash/FreeBSD/hash_buf.c
    db/hash/FreeBSD/hash_func.c
    db/hash/FreeBSD/hash_log2.c
    db/hash/FreeBSD/hash_page.c
    db/hash/FreeBSD/ndbm.c
    db/btree/FreeBSD/bt_close.c
    db/btree/FreeBSD/bt_conv.c
    db/btree/FreeBSD/bt_debug.c
    db/btree/FreeBSD/bt_delete.c
    db/btree/FreeBSD/bt_get.c
    db/btree/FreeBSD/bt_open.c
    db/btree/FreeBSD/bt_overflow.c
    db/btree/FreeBSD/bt_page.c
    db/btree/FreeBSD/bt_put.c
    db/btree/FreeBSD/bt_search.c
    db/btree/FreeBSD/bt_seq.c
    db/btree/FreeBSD/bt_split.c
    db/btree/FreeBSD/bt_utils.c
    db/recno/FreeBSD/rec_close.c
    db/recno/FreeBSD/rec_delete.c
    db/recno/FreeBSD/rec_get.c
    db/recno/FreeBSD/rec_open.c
    db/recno/FreeBSD/rec_put.c
    db/recno/FreeBSD/rec_search.c
    db/recno/FreeBSD/rec_seq.c
    db/recno/FreeBSD/rec_utils.c
    db/mpool/FreeBSD/mpool.c
    gen/devname.c                # _devname / _devname_r
    gen/pwcache.c                # _user_from_uid / _group_from_gid
    gen/uname.c                  # _uname
    gen/FreeBSD/exec.c           # _execvp / _execv / _execlp
    gen/FreeBSD/basename.c       # _basename
    stdlib/FreeBSD/putenv.c      # _putenv
    gen/FreeBSD/sysctl.c         # _sysctl
    gen/FreeBSD/sysctlbyname.c   # _sysctlbyname
    gen/FreeBSD/sysctlnametomib.c # _sysctlnametomib
    gen/FreeBSD/clock.c          # _clock
    gen/FreeBSD/getlogin.c       # _getlogin / _getlogin_r
    gen/pd_getpwent.c            # getpwnam/getpwuid (/etc/passwd reader; no libinfo)
    gen/getusershell.c           # getusershell/setusershell/endusershell (/etc/shells)
    string/FreeBSD/wcscmp.c      # _wcscmp (wcscoll)
    stdlib/FreeBSD/imaxabs.c     # _imaxabs (inttypes)
    stdlib/FreeBSD/strtoimax.c   # _strtoimax / _strtoimax_l (scanf)
    stdlib/FreeBSD/strtoumax.c   # _strtoumax / _strtoumax_l (scanf)
    stdio/FreeBSD/mktemp.c       # _mkstemp / _mkdtemp
    stdio/FreeBSD/setbuf.c       # _setbuf (calls setvbuf)
    stdio/FreeBSD/setbuffer.c    # _setbuffer / _setlinebuf (calls setvbuf)
    stdio/FreeBSD/setvbuf.c      # _setvbuf
    string/FreeBSD/rindex.c      # _rindex
    stdtime/FreeBSD/localtime.c  # _localtime / _mktime / _tzset (tzfile reader)
    stdtime/FreeBSD/strftime.c   # _strftime
    stdtime/FreeBSD/asctime.c    # _asctime
    stdtime/timezone_unix03.c    # _st_get_timezone / _st_set_timezone (strftime %z)
    gen/pd_notify_stub.c         # notify_post shim (real libnotify needs XPC)
    gen/pd_getgrent.c            # getgrnam/getgrgid/getgrouplist (/etc/group reader; no libinfo)
    gen/pd_netdb_stub.c          # gethostbyname/h_errno/hstrerror (no libinfo/libresolv yet)
    gen/pd_getservbyname.c       # getservbyname (/etc/services reader; no libinfo)
    libm/pd_libm_sincos_stret.c  # __sincos*_stret / __sincospi*_stret (Apple-only wrappers, not in openlibm; -fno-builtin below)
    libm/pd_libm_exp10.c         # __exp10 / __exp10f (Apple-only base-10 exp, not in openlibm)
    gen/clock_gettime.c          # _clock_gettime / _clock_settime
    gen/FreeBSD/sethostname.c    # _sethostname
    net/FreeBSD/inet_ntoa.c      # _inet_ntoa
    net/inet_ntop.c              # _inet_ntop (inet_ntoa's formatter)
    net/inet_pton.c              # _inet_pton (libresolv res_init/dns.c server-address parsing)
    string/FreeBSD/wmemcmp.c     # _wmemcmp (TRE regexec wide compares)
    string/FreeBSD/strcasestr.c  # _strcasestr
    string/FreeBSD/strspn.c      # _strspn
    string/FreeBSD/memmem.c      # _memmem (file(1) magic buffer scans)
    string/FreeBSD/wmemcpy.c     # _wmemcpy
    string/FreeBSD/wmemchr.c     # _wmemchr
    string/FreeBSD/wcscat.c      # _wcscat
    string/FreeBSD/wcsrchr.c     # _wcsrchr
    string/FreeBSD/wcstok.c      # _wcstok
    locale/FreeBSD/wcstol.c      # _wcstol
    string/NetBSD/memset_s.c     # _memset_s
    gen/backtrace.c               # _backtrace/_backtrace_symbols/_backtrace_symbols_fd (dbus's execinfo.h use)
    gen/FreeBSD/getpeereid.c     # _getpeereid (dbus's unix-socket peer credential auth)
    gen/FreeBSD/alarm.c          # _alarm
    gen/FreeBSD/fmtcheck.c       # _fmtcheck (file(1) locale-safe strerror formatting)
    secure/strcat_chk.c          # ___strcat_chk
    # POSIX regex: Apple's TRE engine (regcomp/regexec) + BSD regerror.
    regex/FreeBSD/regerror.c
    regex/TRE/lib/regcomp.c
    regex/TRE/lib/regexec.c
    regex/TRE/lib/tre-ast.c
    regex/TRE/lib/tre-compile.c
    regex/TRE/lib/tre-match-backtrack.c
    regex/TRE/lib/tre-match-parallel.c
    regex/TRE/lib/tre-mem.c
    regex/TRE/lib/tre-parse.c
    regex/TRE/lib/tre-stack.c
    stdlib/FreeBSD/qsort.c
    stdlib/FreeBSD/qsort_r.c     # _qsort_r (also backs qsort_b for fts)
    stdlib/FreeBSD/heapsort_r.c  # ___heapsort_r (qsort_r deep-recursion fallback)
    stdlib/FreeBSD/tsearch.c     # _tsearch
    stdlib/FreeBSD/tfind.c       # _tfind
    stdlib/FreeBSD/tdelete.c     # _tdelete
    stdlib/FreeBSD/twalk.c       # _twalk
    stdlib/FreeBSD/system.c      # __system (system/_system aliased via __weak_reference)
    stdio/FreeBSD/remove.c       # _remove
    stdio/FreeBSD/fwrite.c       # _fwrite
    stdio/FreeBSD/dprintf.c      # _dprintf/_dprintf_l
    stdio/FreeBSD/vdprintf.c     # _vdprintf/_vdprintf_l (dprintf's underlying engine)
    stdlib/FreeBSD/realpath.c
    util/pty.c                   # _openpty/_forkpty
    util/login_tty.c             # _login_tty (forkpty's child-side helper)
    emulated/statvfs.c           # _statvfs/_fstatvfs
    secure/memcpy_chk.c
    secure/memset_chk.c
    darwin/subsystem.c
    stdlib/FreeBSD/heapsort.c    # _heapsort
    stdlib/FreeBSD/getenv.c      # _getenv/_setenv/_unsetenv
    gen/FreeBSD/sysctlbyname.c   # _sysctlbyname (-> __sysctlbyname syscall)
    sys/crt_externs.c            # _NSGetEnviron/_NSGetArgv/_NSGetArgc/_NSGetProgname
    stdlib/FreeBSD/exit.c        # _exit/exit (calls __cxa_finalize then the raw syscall)
    stdlib/FreeBSD/atexit.c      # __cxa_atexit/__cxa_finalize/__cxa_finalize_ranges/atexit
    stdlib/FreeBSD/abort.c       # _abort
    gen/FreeBSD/assert.c         # __assert_rtn
    secure/chk_fail.c            # ___chk_fail_overflow (needs mach-o/dyld_priv.h, now vendored)
    darwin/pd_os_crash.c         # __os_crash (minimal real report-then-let-caller-trap; see file comment)
    darwin/pd_unix2003.c         # $UNIX2003 <-> plain forwarders; see file comment
    gen/FreeBSD/arc4random.c     # corecrypto-backed arc4random/arc4random_buf/arc4random_uniform
    gen/FreeBSD/daemon.c         # _daemon$1050
    gen/FreeBSD/signal.c         # _signal (libmalloc's pguard_malloc.c installs a SIGSEGV handler)
    gen/sigsetops.c              # sigemptyset/sigfillset/sigaddset/sigdelset/sigismember
    compat-43/sigaltstk.c        # _sigaltstack public wrapper over the raw syscall
    emulated/bsd_signal.c        # _bsd_signal (POSIX legacy signal wrapper)
    sys/sigaction.c              # __sigaction_nobind (signal.c's underlying raw syscall wrapper)
    gen/raise.c                  # _raise (signal.c's SIG_DFL/SIG_IGN cleanup path)
    gen/getloadavg.c             # _getloadavg
    gen/FreeBSD/sleep.c          # _sleep (pguard_malloc.c's crash-report-wait path)
    gen/nanosleep.c              # _nanosleep (sleep.c's underlying wrapper)
    gen/FreeBSD/isatty.c         # _isatty (pguard_malloc.c's tty-detection for its diagnostic output)
    sys/OpenBSD/stack_protector.c # __stack_chk_fail (libplatform's string_io.c is built WITH stack protector)
    stdlib/FreeBSD/atoi.c      # _atoi
    stdlib/FreeBSD/strtol.c      # _strtol (libmalloc's env-var parsing, e.g. MallocNanoZone)
    stdlib/FreeBSD/strtoll.c     # _strtoll
    stdlib/FreeBSD/atoll.c       # _atoll/_atoll_l
    stdlib/FreeBSD/atol.c        # _atol/_atol_l
    stdlib/FreeBSD/strtoul.c     # _strtoul/_strtoul_l
    stdlib/FreeBSD/strtoull.c    # _strtoull/_strtoull_l
    stdlib/FreeBSD/rand.c        # _rand/_srand
    stdlib/FreeBSD/getopt.c      # _getopt/_optarg/_optind
    stdlib/FreeBSD/getopt_long.c # _getopt_long/_getopt_long_only (file, curl CLI arg parsing)
    stdlib/FreeBSD/bsearch.c     # _bsearch
    gen/thread_stack_pcs.c       # _thread_stack_pcs (libmalloc's pguard/debug backtrace capture)
    gen/FreeBSD/usleep.c         # _usleep
    gen/FreeBSD/gethostname.c    # _gethostname
    gen/FreeBSD/getprogname.c    # _getprogname (malloc_printf.c's diagnostic prefix)
    gen/FreeBSD/time.c           # _time
    stdtime/FreeBSD/difftime.c   # _difftime
    gen/FreeBSD/times.c          # _times
    gen/FreeBSD/ttyname.c        # _ttyname_r
    gen/FreeBSD/dirname.c        # _dirname
    gen/FreeBSD/fnmatch.c        # _fnmatch
    sys/gettimeofday.c           # _gettimeofday
    sys/settimeofday.c           # _settimeofday
    secure/memmove_chk.c         # ___memmove_chk
    secure/strcpy_chk.c          # ___strcpy_chk
    secure/strlcpy_chk.c         # ___strlcpy_chk
    secure/strncpy_chk.c         # ___strncpy_chk
    secure/sprintf_chk.c         # ___sprintf_chk
    gen/FreeBSD/termios.c        # _tcgetattr
    gen/setlogin.c               # _setlogin
    net/pd_ether_addr.c          # _ether_aton/_ether_ntoa
    string/FreeBSD/strnstr.c     # _strnstr
    os/pd_assumes.c               # os_assumes/os_assert real-contract impl (see file comment: real os/assumes.c needs firehose/os_log private ABI logd doesn't back here)
    emulated/lchflags.c           # _lchflags (real launchctl.c)
    emulated/flock.c              # _flock
    uuid/uuidsrc/gen_uuid.c      # uuid_generate/uuid_generate_random/uuid_generate_time (real Libinfo/launchd need the full uuid_* family)
    uuid/uuidsrc/clear.c         # uuid_clear
    uuid/uuidsrc/compare.c       # uuid_compare
    uuid/uuidsrc/copy.c          # uuid_copy
    uuid/uuidsrc/isnull.c        # uuid_is_null
    uuid/uuidsrc/unparse.c       # uuid_unparse/uuid_unparse_lower/uuid_unparse_upper
    uuid/uuidsrc/pack.c          # uuid_pack
    uuid/uuidsrc/unpack.c        # uuid_unpack
    # TODO (need cross-deps beyond the SDK sysroot):
    #  - stdio/FreeBSD/mktemp.c (_mkstemp) needs BSD-internal TRUE/FALSE/bool
    # printf/fprintf/snprintf and the locale/rune chain are NOT deferred anymore
    # - resolved for real in libc_internal_static below (findfp.c/vfprintf.c/
    # xlocale.c/table.c/gdtoa/... the whole "internal libc" stdio+locale
    # sub-project this comment used to point at).
)
set_target_properties(libc_static PROPERTIES EXCLUDE_FROM_ALL TRUE)

set_source_files_properties(
    uuid/uuidsrc/gen_uuid.c
    uuid/uuidsrc/clear.c
    uuid/uuidsrc/compare.c
    uuid/uuidsrc/copy.c
    uuid/uuidsrc/isnull.c
    uuid/uuidsrc/unparse.c
    uuid/uuidsrc/pack.c
    uuid/uuidsrc/unpack.c
    PROPERTIES COMPILE_DEFINITIONS "HAVE_STDINT_H=1;HAVE_UNISTD_H=1;HAVE_STDLIB_H=1")
# HAVE_NET_IF_H/HAVE_SYS_SOCKET_H intentionally left undefined: gen_uuid.c's
# get_node_id() MAC-address lookup needs sockaddr_dl/SIOCGIFCONF plumbing not
# confirmed here; leaving it off makes uuid_generate_time() fall back to a
# random node id, which is real gen_uuid.c's own documented fallback path
# when no hardware address is available (see get_node_id() <= 0 branch).

set_source_files_properties(libm/pd_libm_sincos_stret.c PROPERTIES
    COMPILE_OPTIONS "-fno-builtin-sincos;-fno-builtin-sincosf")

get_target_property(_libc_copts libc_static COMPILE_OPTIONS)
list(REMOVE_ITEM _libc_copts -nostdinc)
set_target_properties(libc_static PROPERTIES COMPILE_OPTIONS "${_libc_copts}")
target_compile_options(libc_static PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_compile_definitions(libc_static PRIVATE
    _FORTIFY_SOURCE=0
    PR_13085474_CHECK=0
    PUREDARWIN_EARLY_USERLAND=1)
# bridgeos availability shim (reuse libplatform's), force-included for C: several
# private headers (os/lock_private.h) use __API_AVAILABLE(...bridgeos(x)...) which
# neither the SDK nor PD define.
target_compile_options(libc_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/pd_bridgeos_compat.h>)
target_compile_definitions(libc_static PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1)
# xnu_private_headers for the pthread QoS chain (pthread/priority_private.h in
# usr/local/include). dyld links this alongside -isysroot too.
target_link_libraries(libc_static PRIVATE xnu_private_headers)
target_link_libraries(libc_static PUBLIC openlibm_static)
add_dependencies(libc_static libc_features libsystem_kernel_static)
target_include_directories(libc_static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/include/${PUREDARWIN_ARCH}   # generated libc-features.h
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}               # <os/assumes.h> (os/assumes.h), <secure/...>
    ${CMAKE_CURRENT_SOURCE_DIR}/include       # libc's own public headers for libc sources
    ${CMAKE_CURRENT_SOURCE_DIR}/darwin        # libc_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/darwin/pd_compat  # abort.c: <pthread_workqueue.h> forwarder
    ${CMAKE_CURRENT_SOURCE_DIR}/fbsdcompat     # namespace.h / un-namespace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stdio/FreeBSD   # local.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/FreeBSD     # telldir.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gen              # utmpx-darwin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/locale          # xlocale_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/locale/FreeBSD   # setlocale.h + rune internals
    ${CMAKE_CURRENT_SOURCE_DIR}/stdtime/FreeBSD   # timelocal.h (locale chain)
    ${CMAKE_CURRENT_SOURCE_DIR}/../corecrypto/include # corecrypto-backed arc4random
    ${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/private   # _simple.h (subsystem.c)
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/private       # pthread/tsd_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/private_include  # os/tsd.h
    # atexit.c: os/alloc_once_private.h (reconstructed, shared with pthread_static)
    # + os/base_private.h (vendored from ravynOS's userspace Libsystem copy).
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/compat-include
    # atexit.c: Block_private.h (libdispatch's userspace BlocksRuntime copy).
    ${CMAKE_CURRENT_SOURCE_DIR}/../libdispatch/src/BlocksRuntime
    # chk_fail.c: mach-o/dyld_priv.h (not in the SDK) from the vendored dyld tree.
    ${CMAKE_SOURCE_DIR}/src/Libraries/dyld/upstream/include
    # Prefer libsystem_kernel's userspace mach headers over xnu's kernel mach.h.
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/mach
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/libkern
    ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/libkern/libkern
    ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/bsd
    # strtol.c/strtoul.c/strtoull.c: os/tsd.h (mach), System/machine/cpu_capabilities.h
    # redirect (libmalloc's compat-include), stack_logging.h (libmalloc/private,
    # pulled transitively).
    ${CMAKE_CURRENT_SOURCE_DIR}/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libmalloc/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libmalloc/compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../CrashReporterClient/include  # os/pd_assumes.c
)
set_source_files_properties(stdlib/FreeBSD/exit.c stdlib/FreeBSD/atexit.c
    stdlib/FreeBSD/abort.c gen/FreeBSD/assert.c gen/FreeBSD/signal.c gen/FreeBSD/sleep.c
    gen/FreeBSD/usleep.c gen/FreeBSD/getprogname.c gen/FreeBSD/termios.c
    stdio/FreeBSD/putwchar.c stdio/FreeBSD/putwc.c stdio/FreeBSD/fputwc.c
    stdio/FreeBSD/fputws.c stdio/FreeBSD/getwchar.c stdio/FreeBSD/getwc.c
    stdio/FreeBSD/fgetwc.c stdio/FreeBSD/fgetws.c stdio/FreeBSD/fgetwln.c
    stdio/FreeBSD/ungetwc.c stdio/FreeBSD/fwide.c stdio/FreeBSD/wprintf.c
    stdio/FreeBSD/vfwprintf.c stdio/FreeBSD/vswprintf.c stdio/FreeBSD/swprintf.c
    stdio/FreeBSD/freopen.c gen/FreeBSD/setmode.c
    db/db/FreeBSD/db.c db/hash/FreeBSD/hash.c db/hash/FreeBSD/hash_bigkey.c
    db/hash/FreeBSD/hash_buf.c db/hash/FreeBSD/hash_func.c
    db/hash/FreeBSD/hash_log2.c db/hash/FreeBSD/hash_page.c
    db/hash/FreeBSD/ndbm.c db/btree/FreeBSD/bt_close.c
    db/btree/FreeBSD/bt_conv.c db/btree/FreeBSD/bt_debug.c
    db/btree/FreeBSD/bt_delete.c db/btree/FreeBSD/bt_get.c
    db/btree/FreeBSD/bt_open.c db/btree/FreeBSD/bt_overflow.c
    db/btree/FreeBSD/bt_page.c db/btree/FreeBSD/bt_put.c
    db/btree/FreeBSD/bt_search.c db/btree/FreeBSD/bt_seq.c
    db/btree/FreeBSD/bt_split.c db/btree/FreeBSD/bt_utils.c
    db/recno/FreeBSD/rec_close.c db/recno/FreeBSD/rec_delete.c
    db/recno/FreeBSD/rec_get.c db/recno/FreeBSD/rec_open.c
    db/recno/FreeBSD/rec_put.c db/recno/FreeBSD/rec_search.c
    db/recno/FreeBSD/rec_seq.c db/recno/FreeBSD/rec_utils.c
    db/mpool/FreeBSD/mpool.c
    stdlib/FreeBSD/random.c
    stdio/FreeBSD/tmpfile.c
    stdio/FreeBSD/fwrite.c stdio/FreeBSD/remove.c
    stdio/FreeBSD/dprintf.c stdio/FreeBSD/vdprintf.c
    stdlib/FreeBSD/system.c
    stdlib/FreeBSD/tsearch.c stdlib/FreeBSD/tfind.c
    stdlib/FreeBSD/tdelete.c stdlib/FreeBSD/twalk.c
    locale/FreeBSD/mbsinit.c locale/FreeBSD/wcstombs.c
    util/pty.c util/login_tty.c
    PROPERTIES COMPILE_FLAGS "-include${CMAKE_CURRENT_SOURCE_DIR}/fbsdcompat/_fbsd_compat_.h -D__DBINTERFACE_PRIVATE=1")
# wcscoll needs the collate internals (__collate_lookup_which etc.), gated
# behind __LIBC__ in collate.h - same as fnmatch.c above. strxfrm/wcsxfrm need
# __collate_xfrm/__collate_mbstowcs (also __LIBC__-gated, defined in collate.c).
set_source_files_properties(
    string/FreeBSD/wcscoll.c
    string/FreeBSD/strxfrm.c
    string/FreeBSD/wcsxfrm.c
    PROPERTIES COMPILE_FLAGS "-D__LIBC__=1")
set_source_files_properties(sys/OpenBSD/stack_protector.c
    PROPERTIES COMPILE_FLAGS "-include${CMAKE_CURRENT_SOURCE_DIR}/fbsdcompat/_fbsd_compat_.h -fno-stack-protector")

# rand.c / ttyname.c use the underscore syscall/pthread aliases (_open/_read/
# _close/_fstat/_pthread_*/__isthreaded) -> the _fbsd_compat_.h macro shim maps
# them to the SDK-declared real functions. ttyname.c additionally needs the
# TSD-key infra (__LIBC_PTHREAD_KEY_TTYNAME + pthread_key_init_np), which the
# shim now also provides (PD's libc/include/sys/cdefs.h that would supply them is
# shadowed here by xnu's PrivateHeaders/sys/cdefs.h, which lacks the key macros).
set_source_files_properties(
    stdlib/FreeBSD/rand.c gen/FreeBSD/ttyname.c gen/FreeBSD/exec.c
    stdtime/FreeBSD/localtime.c stdtime/FreeBSD/strftime.c stdtime/FreeBSD/asctime.c
    stdio/FreeBSD/setbuf.c stdio/FreeBSD/setbuffer.c stdio/FreeBSD/setvbuf.c
    gen/FreeBSD/basename.c gen/FreeBSD/gethostname.c gen/FreeBSD/daemon.c
    PROPERTIES COMPILE_FLAGS "-include${CMAKE_CURRENT_SOURCE_DIR}/fbsdcompat/_fbsd_compat_.h")
# fnmatch.c needs the collate internals (__collate_*) gated behind __LIBC__ in
# collate.h; no underscore-alias shim required.
set_source_files_properties(
    gen/FreeBSD/fnmatch.c
    PROPERTIES COMPILE_FLAGS "-D__LIBC__=1")
# glob.c needs both: the underscore syscall aliases (_open/_stat/_readdir/...) AND
# the collate range compare (__collate_range_cmp, __LIBC__-gated in collate.h).
set_source_files_properties(
    gen/FreeBSD/glob.c
    PROPERTIES COMPILE_FLAGS "-include${CMAKE_CURRENT_SOURCE_DIR}/fbsdcompat/_fbsd_compat_.h -D__LIBC__=1")
# mktemp.c uses the BSD-internal bool + TRUE/FALSE spellings; supply <stdbool.h>
# and the uppercase aliases (plus the syscall-alias shim for _open/_fstat/etc).
set_source_files_properties(
    stdio/FreeBSD/mktemp.c
    PROPERTIES COMPILE_FLAGS "-include${CMAKE_CURRENT_SOURCE_DIR}/fbsdcompat/_fbsd_compat_.h -include stdbool.h -DTRUE=true -DFALSE=false")

# TRE (regcomp/regexec) ships its own pre-generated config.h/tre-config.h in
# regex/TRE/; regerror.c uses the same tre.h plus the old BSD utils.h.
set_source_files_properties(
    regex/FreeBSD/regerror.c
    regex/TRE/lib/regcomp.c
    regex/TRE/lib/regexec.c
    regex/TRE/lib/tre-ast.c
    regex/TRE/lib/tre-compile.c
    regex/TRE/lib/tre-match-backtrack.c
    regex/TRE/lib/tre-match-parallel.c
    regex/TRE/lib/tre-mem.c
    regex/TRE/lib/tre-parse.c
    regex/TRE/lib/tre-stack.c
    PROPERTIES COMPILE_FLAGS "-DHAVE_CONFIG_H -I${CMAKE_CURRENT_SOURCE_DIR}/regex/TRE -I${CMAKE_CURRENT_SOURCE_DIR}/regex/TRE/lib -I${CMAKE_CURRENT_SOURCE_DIR}/regex/FreeBSD")

add_darwin_static_library(libc_internal_static)
set_target_properties(libc_internal_static PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(libc_internal_static PRIVATE
    gen/FreeBSD/opendir.c        # _opendir$INODE64
    gen/FreeBSD/readdir.c        # _readdir$INODE64 / _readdir_r$INODE64
    gen/FreeBSD/scandir.c        # _scandir$INODE64 / _alphasort$INODE64
    gen/FreeBSD/closedir.c       # _closedir
    gen/FreeBSD/telldir.c        # telldir/seekdir support used by the above
    gen/FreeBSD/getcwd.c         # _getcwd / ___private_getcwd
    gen/FreeBSD/getpagesize.c    # _getpagesize (pulled by telldir)
    stdlib/FreeBSD/reallocf.c    # _reallocf (pulled by telldir/getcwd)
    stdlib/FreeBSD/merge.c       # _mergesort (pulled by scandir/getcwd)
    locale/FreeBSD/table.c       # ___maskrune / __DefaultRuneLocale (C-locale rune table)
    gen/dirfd.c                  # _dirfd
    locale/xlocale.c             # __c_locale/__global_locale/__locale_key + newlocale/uselocale family
    locale/isctype.c             # externs isspace_l & co (ctype.h inlines, extern'd via _EXTERNALIZE_CTYPE_INLINES_TOP_)
    stdio/FreeBSD/findfp.c        # __sF[]/__stderrp, _finddata, __sinit
    stdio/FreeBSD/stdio.c         # __sread/__swrite/__sseek/__sclose (fd-backed I/O callbacks)
    stdio/FreeBSD/makebuf.c       # __swsetup's buffer sizing (fstat-based)
    stdio/FreeBSD/wsetup.c        # __swsetup (write-mode buffer setup)
    stdio/FreeBSD/refill.c        # __srefill (read-mode refill; pulled in transitively)
    stdio/FreeBSD/fvwrite.c       # __sfvwrite (vfprintf's underlying buffered writer)
    stdio/FreeBSD/fflush.c        # __sflush / fflush (used by exit's stdio-flush + fvwrite)
    stdio/FreeBSD/fpurge.c        # _fpurge (zsh redirection cleanup)
    stdio/FreeBSD/fclose.c        # fclose (fwalk's flush-and-close path referenced by findfp.c)
    stdio/FreeBSD/perror.c        # _perror (init error reporting)
    stdio/FreeBSD/vfprintf.c      # __vfprintf (the real BSD engine; xprintf variant is #if 0'd out)
    stdio/FreeBSD/vfwprintf.c     # __vfwprintf (wide engine; needs gdtoa.h like vfprintf)
    stdio/FreeBSD/fprintf.c       # _fprintf
    stdio/FreeBSD/printf.c        # _printf
    stdio/FreeBSD/vsnprintf.c     # _vsnprintf (snprintf's engine)
    stdio/FreeBSD/snprintf.c      # _snprintf
    secure/snprintf_chk.c         # ___snprintf_chk (libmalloc's pguard_malloc.c FORTIFY call)
    secure/vsnprintf_chk.c        # ___vsnprintf_chk (snprintf_chk's underlying engine)
    stdio/FreeBSD/_flock_stub.c   # _flockfile/_funlockfile (vfprintf's FILE locking)
    locale/FreeBSD/setrunelocale.c # __setrunelocale (xlocale's uselocale/newlocale LC_CTYPE switch)
    locale/FreeBSD/setlocale.c    # __get_locale_env/__detect_path_locale (setlocale's env-var lookup)
    locale/FreeBSD/collate.c      # __collate_load_tables
    locale/FreeBSD/collcmp.c      # __collate_range_cmp (fnmatch/regex)
    locale/FreeBSD/wctype.c       # _wctype
    string/FreeBSD/strcoll.c      # _strcoll / _strcoll_l (collate compare)
    string/FreeBSD/wcscoll.c      # _wcscoll_l (wide collate compare)
    locale/FreeBSD/wctob.c        # _wctob / _wctob_l (scanf)
    locale/FreeBSD/wcwidth.c      # _wcwidth / _wcwidth_l
    stdio/FreeBSD/rget.c          # ___srget (getc/__srget refill)
    stdio/FreeBSD/ungetc.c        # ___ungetc (scanf pushback)
    stdio/FreeBSD/getc.c          # _getc
    stdio/FreeBSD/getchar.c       # _getchar
    stdio/FreeBSD/vsscanf.c       # _vsscanf_l / _vsscanf
    stdio/FreeBSD/vfscanf.c       # __svfscanf / __svfscanf_l (scanf engine)
    locale/FreeBSD/lnumeric.c     # __numeric_load_locale
    locale/FreeBSD/lmonetary.c    # __monetary_load_locale
    locale/FreeBSD/lmessages.c    # __messages_load_locale
    stdtime/FreeBSD/timelocal.c   # __time_load_locale
    locale/FreeBSD/none.c         # __none_{mbrtowc,mbsinit,mbsnrtowcs,wcrtomb,wcsnrtombs} (C-locale "no conversion" charset backend)
    locale/FreeBSD/mbsinit.c      # _mbsinit / _mbsinit_l
    locale/FreeBSD/wcstombs.c     # _wcstombs / _wcstombs_l
    locale/FreeBSD/tolower.c      # _tolower/_toupper (non-locale-suffixed forms)
    locale/FreeBSD/toupper.c
    locale/FreeBSD/nomacros.c     # externs __istype/__maskrune/__isctype (ctype.h base inlines, non-_l)
    gen/FreeBSD/sysconf.c         # _sysconf (findfp.c's __sinit uses it for buffer sizing)
    gen/FreeBSD/sysctl.c          # _sysctl (sysconf/getpagesize's underlying syscall wrapper)
    darwin/pd_ffs.c               # ffs (real __builtin_ffs; see file comment)
    locale/FreeBSD/rune.c         # _Read_RuneMagi (locale-file rune-table parser)
    locale/FreeBSD/ascii.c        # __ascii_init
    locale/FreeBSD/utf2.c         # __UTF2_init
    locale/FreeBSD/big5.c         # __BIG5_init
    locale/FreeBSD/gb2312.c       # __GB2312_init
    locale/FreeBSD/gb18030.c      # __GB18030_init
    locale/FreeBSD/gbk.c          # __GBK_init
    locale/FreeBSD/mskanji.c      # __MSKanji_init
    stdio/FreeBSD/fopen.c         # _fopen (collate/setrunelocale's locale-file open)
    stdio/FreeBSD/fdopen.c        # _fdopen (fopen's underlying FILE* wrapper)
    stdio/FreeBSD/getdelim.c      # _getdelim
    stdio/FreeBSD/getline.c       # _getline
    stdio/FreeBSD/fgets.c         # _fgets
    stdio/FreeBSD/fread.c         # _fread (collate table loading)
    string/FreeBSD/wcsncmp.c      # _wcsncmp (collate's chainsearch)
    string/FreeBSD/strerror.c     # _strerror (collate's error-message path)
    string/FreeBSD/strcasecmp.c   # _strcasecmp/_strncasecmp
    string/FreeBSD/strcspn.c      # _strcspn
    string/FreeBSD/strpbrk.c      # _strpbrk
    string/FreeBSD/strndup.c      # _strndup
    string/FreeBSD/strsignal.c    # _strsignal
    locale/FreeBSD/mbrtowc.c      # _mbrtowc_l (collate's equiv-class matching)
    locale/FreeBSD/mbsrtowcs.c    # _mbsrtowcs_l (collate's mbstowcs)
    locale/FreeBSD/ldpart.c       # __part_load_locale / __ldpart_free_extra (shared LC_* file-loading engine)
    locale/FreeBSD/fix_grouping.c # __fix_locale_grouping_str (numeric/monetary grouping)
    locale/FreeBSD/runetype.c     # __runetype/__runetype_l (nomacros.c's non-cached-rune path)
    stdio/FreeBSD/xprintf.c       # __xvprintf (vfprintf.c's %n$ positional-arg / xprintf-domain hook)
    stdio/FreeBSD/fwalk.c         # _fwalk (findfp.c's atexit walk-all-FILEs callback)
    stdio/FreeBSD/fileno.c        # _fileno (rune.c's Read_RuneMagi uses it to check regular-file-ness)
    stdio/FreeBSD/rewind.c        # _rewind (Read_RuneMagi's re-read-after-check)
    stdio/xprintf_domain.c        # __xprintf_domain_{once,init} (xprintf.c's %n$ domain registry)
    locale/FreeBSD/wcsrtombs.c    # _wcsrtombs_l (vfprintf's wide-string-to-multibyte conversion)
    locale/FreeBSD/wcrtomb.c      # _wcrtomb_l
    stdio/FreeBSD/asprintf.c      # _asprintf_l (vfprintf's os_assert_asprintf path)
    locale/FreeBSD/utf8.c         # __UTF8_init (setrunelocale's charset table)
    stdio/FreeBSD/printf-pos.c    # __find_arguments (vfprintf's %n$ positional-argument support)
    gen/errlst.c                  # sys_errlist/sys_nerr (strerror's message table)
    stdio/FreeBSD/flags.c         # __sflags (fdopen's mode-string parser)
    locale/FreeBSD/mbsnrtowcs.c   # __mbsnrtowcs_std (setrunelocale's stateless-conversion fallback)
    gen/FreeBSD/err.c             # _errx/_e_visprintf (xprintf.c's malformed-format-string abort path)
    stdio/FreeBSD/vasprintf.c     # _vasprintf/_vasprintf_l (asprintf.c + err.c's underlying engine)
    stdio/FreeBSD/sprintf.c       # _sprintf (err.c's _e_visprintf)
    stdio/FreeBSD/sscanf.c        # _sscanf
    stdio/FreeBSD/fscanf.c        # _fscanf (libfontenc's encparse.c)
    stdio/FreeBSD/funopen.c       # _funopen (cookie-backed stdio)
    stdio/FreeBSD/fmemopen.c      # _fmemopen (memory-backed FILE*)
    stdlib/FreeBSD/atof.c         # _atof (freetype/libXfont2 config parsing)
    stdio/FreeBSD/fputs.c         # _fputs (err.c's _e_visprintf)
    stdio/FreeBSD/ferror.c        # _ferror
    stdio/FreeBSD/feof.c          # _feof (xkbcomp)
    stdio/FreeBSD/clrerr.c        # _clearerr
    stdio/FreeBSD/fgetc.c         # _fgetc
    stdio/FreeBSD/putc.c          # _putc
    stdio/FreeBSD/putchar.c       # _putchar
    stdio/FreeBSD/vprintf.c       # _vprintf
    stdio/FreeBSD/fseek.c         # _fseeko (rewind.c)
    stdio/FreeBSD/fgetpos.c       # _fgetpos (flex table serialization)
    stdio/FreeBSD/fsetpos.c       # _fsetpos (flex table serialization)
    locale/FreeBSD/localeconv.c   # _localeconv/_localeconv_l (vfprintf's grouping_init)
    stdio/FreeBSD/xprintf_str.c
    stdio/FreeBSD/xprintf_int.c    # also chr/ptr
    stdio/FreeBSD/xprintf_float.c
    stdio/FreeBSD/xprintf_quote.c
    stdio/FreeBSD/xprintf_time.c
    stdio/FreeBSD/xprintf_vis.c
    stdio/FreeBSD/xprintf_errno.c
    stdio/FreeBSD/xprintf_hexdump.c
    locale/pd_legacy_rune.c
    locale/FreeBSD/euc.c           # __EUC_init (setrunelocale's charset table)
    locale/FreeBSD/wcsnrtombs.c    # __wcsnrtombs_std (setrunelocale's stateless-conversion fallback)
    stdio/FreeBSD/puts.c           # __puts_null__ (shared "(null)" string; fputs.c is the extern side)
    stdio/FreeBSD/fputc.c          # _fputc (dyld's pd_stubs.c had a fake one; real stdio covers it now)
    stdio/FreeBSD/wbuf.c           # __swbuf (fputc's buffer-full slow path)
    stdio/FreeBSD/vsprintf.c       # _vsprintf_l (sprintf.c's engine)
    stdio/FreeBSD/ftell.c          # _ftello (fseek.c's fseeko)
    gen/FreeBSD/vis.c              # _strvisx (xprintf_vis.c's %V renderer)
    darwin/pd_fpclassify.c         # __fpclassify{f,d,l} (real; see file comment)
    locale/iswctype.c              # externs iswgraph/iswspace/iswalnum/iswcntrl & co (wctype.h inlines, same pattern as isctype.c)
    string/FreeBSD/wcschr.c        # _wcschr (vis.c's multi-byte extra-chars-to-encode scan)
    string/FreeBSD/wcslen.c        # _wcslen
    locale/FreeBSD/mbstowcs.c      # _mbstowcs (vis.c's makeextralist)
    locale/FreeBSD/mbtowc.c        # _mbtowc (vis.c's istrsenvisx)
    locale/FreeBSD/mbrlen.c        # _mbrlen/_mbrlen_l (vfwprintf's __mbsconv)
    locale/FreeBSD/btowc.c         # _btowc/_btowc_l (vfwprintf)
    stdio/FreeBSD/fgetln.c         # _fgetln + __slbexpand (fgetwln)
    stdio/FreeBSD/fwprintf.c       # _fwprintf
    stdio/FreeBSD/vwprintf.c       # _vwprintf
    locale/FreeBSD/wctomb.c        # _wctomb
    locale/FreeBSD/mblen.c         # _mblen (Xlib setlocale/mb path)
    net/FreeBSD/inet_addr.c        # _inet_addr / _inet_aton (Xlib DISPLAY parsing)
    net/FreeBSD/recv.c             # _recv (Xlib socket transport)
    net/FreeBSD/send.c             # _send (curl socket transport)
    net/pd_in6_addr.c              # _in6addr_any / _in6addr_loopback (xterm Xtrans IPv6 transport)
    string/FreeBSD/wcscpy.c        # _wcscpy (collate.c's __collate_wcsdup)
    string/FreeBSD/wcsncpy.c       # _wcsncpy (collate.c's substitute/collating-symbol/equiv-value)
    gdtoa/FreeBSD/gdtoa-dmisc.c
    gdtoa/FreeBSD/gdtoa-dtoa.c
    gdtoa/FreeBSD/gdtoa-gdtoa.c
    gdtoa/FreeBSD/gdtoa-gethex.c
    gdtoa/FreeBSD/gdtoa-gmisc.c
    gdtoa/FreeBSD/gdtoa-hd_init.c
    gdtoa/FreeBSD/gdtoa-hexnan.c
    gdtoa/FreeBSD/gdtoa-misc.c
    gdtoa/FreeBSD/gdtoa-smisc.c
    gdtoa/FreeBSD/gdtoa-strtod.c
    gdtoa/FreeBSD/gdtoa-strtodg.c
    gdtoa/FreeBSD/gdtoa-strtof.c
    gdtoa/FreeBSD/gdtoa-strtopdd.c
    gdtoa/FreeBSD/gdtoa-strtopx.c
    gdtoa/FreeBSD/gdtoa-strtord.c
    gdtoa/FreeBSD/gdtoa-sum.c
    gdtoa/FreeBSD/gdtoa-ulp.c
    gdtoa/FreeBSD/glue.c
    gdtoa/FreeBSD/_hdtoa.c
    gdtoa/FreeBSD/_ldtoa.c
    gdtoa/FreeBSD/machdep_ldisd.c
    gdtoa/FreeBSD/machdep_ldisdd.c
    gdtoa/FreeBSD/machdep_ldisQ.c
    gdtoa/FreeBSD/machdep_ldisx.c
)
target_compile_definitions(libc_internal_static PRIVATE
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    __DARWIN_UNIX03=1
    PRIVATE=1
    _FORTIFY_SOURCE=0
    PR_13085474_CHECK=0
    __LIBC__=1)
target_include_directories(libc_internal_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pd-compat-include   # reconstructed apfs/apfs_fsctl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/include  # libkern/OSAtomic.h (locale refcounts)
    ${CMAKE_CURRENT_SOURCE_DIR}/include             # internal std headers (dirent.h w/ __dd_*)
    ${CMAKE_CURRENT_BINARY_DIR}/include/${PUREDARWIN_ARCH}   # generated libc-features.h
    ${CMAKE_CURRENT_SOURCE_DIR}                      # <os/...>, <secure/...>
    ${CMAKE_CURRENT_SOURCE_DIR}/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/fbsdcompat           # namespace.h / un-namespace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stdio                # xprintf_domain.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/FreeBSD          # telldir.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stdio/FreeBSD        # local.h (internal FILE)
    ${CMAKE_CURRENT_SOURCE_DIR}/locale
    ${CMAKE_CURRENT_SOURCE_DIR}/locale/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtoa/FreeBSD         # gdtoa.h / gdtoaimp.h (xprintf_float.c's dtoa engine)
    ${CMAKE_CURRENT_SOURCE_DIR}/gdtoa                 # arith.h (Apple's pre-generated per-arch arithchk.c output)
    ${CMAKE_CURRENT_SOURCE_DIR}/stdtime/FreeBSD       # timelocal.h (locale chain)
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/mach
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    # <pthread.h> (the dir fns include it) + the whole sys/_pthread/_pthread_*.h
    # family that xnu's sys/types.h references (xnu_header_install omits them).
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/include/pthread   # <pthread.h> itself
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/private           # pthread/tsd_private.h (xlocale)
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/compat-include    # os/base_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/private       # _simple.h (os/assumes.h)
    ${CMAKE_CURRENT_SOURCE_DIR}/../libmalloc/include         # malloc/_malloc.h (stdlib.h)
    ${CMAKE_CURRENT_SOURCE_DIR}/../libmalloc/compat-include  # System/machine/cpu_capabilities.h
    ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/osfmk            # i386/cpu_capabilities.h
    ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/bsd              # sys/wait.h and BSD header tree
)
target_link_libraries(libc_internal_static PRIVATE
    clang_bootstrap_headers      # stddef/stdarg/stdint under -nostdinc
    AvailabilityHeaders
    architecture_headers
    # xnu_private_headers FIRST (PRIVATE mach/* defines) then public.
    xnu_private_headers
    xnu_headers
)
add_dependencies(libc_internal_static libc_features libsystem_kernel_static)
# Force-includes (C only): bridgeos availability shim + the FreeBSD compat shim
# (_fbsd_compat_.h: __isthreaded, and the _close/_fstat/... -> public-name
# remapping the FreeBSD sources expect).
target_compile_options(libc_internal_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/fbsdcompat/_fbsd_compat_.h>
    $<$<COMPILE_LANGUAGE:C>:-idirafter${PUREDARWIN_MACOSX_SDK}/usr/include>)
# The dir sources use the short DIR field names (dd_fd/dd_lock/...); gen/__dirent.h
# maps them onto the internal __dd_* members. Force-include it for those files.
set_source_files_properties(
    gen/FreeBSD/opendir.c gen/FreeBSD/readdir.c
    gen/FreeBSD/closedir.c gen/FreeBSD/telldir.c
    PROPERTIES COMPILE_FLAGS "-include${CMAKE_CURRENT_SOURCE_DIR}/gen/__dirent.h")
