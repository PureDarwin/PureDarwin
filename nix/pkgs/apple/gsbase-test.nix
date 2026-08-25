{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
in
stdenv.mkDerivation {
  pname = "puredarwin-gsbase-test";
  version = "1";
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cat > gsbase-test.c <<'EOF'
    /*
     * Does thread_info(THREAD_IDENTIFIER_INFO) report this thread's TSD base?
     *
     * Wine's init_handler() (dlls/ntdll/unix/signal_x86_64.c) reads it once via
     * mac_thread_gsbase() and then, on EVERY signal, does
     * _thread_set_tsd_base(that value). It does not check for failure, so a
     * thread_info() that fails or reports zero silently arms a crash that only
     * fires when a signal arrives: gsbase becomes 0, and the first
     * pthread_getspecific(key) then reads absolute key*8.
     *
     * thread_handle must equal the gs base, which is what %gs:0x0 holds
     * (TSD slot 0 is the pthread_t, and it points at its own tsd array).
     */
    #include <mach/mach.h>
    #include <stdlib.h>
    #include <string.h>
    #include <mach/thread_info.h>
    #include <pthread.h>
    #include <signal.h>
    #include <errno.h>
    #include <stdio.h>

    static uint64_t read_gsbase_slot0(void)
    {
        uint64_t v;
        __asm__ volatile("movq %%gs:0x0, %0" : "=r"(v));
        return v;
    }

    /*
     * TSD slot 6. This is what NtCurrentTeb() compiles to on x86_64
     * ("movq %gs:0x30,%rax"), and Wine depends on it twice over:
     *
     *   - In Wine code, gsbase is the TEB itself, so %gs:0x30 reads
     *     TEB.NtTib.Self, which points back at the TEB.
     *   - Inside a signal handler, init_handler() has just repointed gsbase at
     *     the pthread TSD base, so %gs:0x30 reads pthread TSD slot 6 instead.
     *
     * save_context() calls amd64_thread_data()->dr0 in the handler, which is
     * &NtCurrentTeb()->GdiTebBatch at offset 0x2f0. If slot 6 reads back as 0
     * that computes to the literal address 0x2f0 and faults - which is the
     * wine-preloader crash loop.
     */
    static uint64_t read_gsbase_slot6(void)
    {
        uint64_t v;
        __asm__ volatile("movq %%gs:0x30, %0" : "=r"(v));
        return v;
    }

    static void report(const char *who)
    {
        struct thread_identifier_info tiinfo;
        mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
        mach_port_t self = mach_thread_self();
        kern_return_t kr;

        memset(&tiinfo, 0, sizeof(tiinfo));
        kr = thread_info(self, THREAD_IDENTIFIER_INFO,
                         (thread_info_t)&tiinfo, &count);
        mach_port_deallocate(mach_task_self(), self);

        printf("[%s]\n", who);
        printf("  thread_info kr      = %d (%s)\n", kr,
               kr == KERN_SUCCESS ? "KERN_SUCCESS" : "FAILED");
        printf("  thread_id           = 0x%llx\n",
               (unsigned long long)tiinfo.thread_id);
        printf("  thread_handle       = 0x%llx  <- Wine uses this as gsbase\n",
               (unsigned long long)tiinfo.thread_handle);
        printf("  pthread_self()      = %p\n", (void *)pthread_self());
        printf("  %%gs:0x0 (tsd slot 0) = 0x%llx\n",
               (unsigned long long)read_gsbase_slot0());
        printf("  %%gs:0x30 (tsd slot 6) = 0x%llx  <- what NtCurrentTeb() reads\n",
               (unsigned long long)read_gsbase_slot6());
        if (read_gsbase_slot6() == 0)
            printf("  NOTE: slot 6 is 0. In a signal handler Wine would compute"
                   " NtCurrentTeb()->GdiTebBatch as 0x2f0 and fault there.\n");
        if (kr == KERN_SUCCESS && tiinfo.thread_handle == 0)
            printf("  VERDICT: thread_info succeeded but handle is 0 -"
                   " Wine will zero gsbase on the first signal.\n");
        else if (kr != KERN_SUCCESS)
            printf("  VERDICT: thread_info FAILED -"
                   " mac_thread_gsbase() returns NULL, same outcome.\n");
        else
            printf("  VERDICT: handle looks usable.\n");
        fflush(stdout);
    }

    /* Wine installs its handlers SA_ONSTACK and registers a sigaltstack inside
     * the 64K-aligned block that also holds the TEB, because get_current_teb()
     * is literally (TEB *)(rsp & ~0xffff). If the handler does NOT run on that
     * altstack, Wine reads its thread_data from whatever the main stack masks
     * to, and the pthread_teb it then feeds to _thread_set_tsd_base() is
     * garbage - which is one way gsbase ends up zero. */
    static stack_t g_altstack;

    static void handler(int sig)
    {
        unsigned long rsp;
        char *lo = (char *)g_altstack.ss_sp;
        char *hi = lo + g_altstack.ss_size;
        int on;

        (void)sig;
        __asm__ volatile("movq %%rsp, %0" : "=r"(rsp));
        on = ((char *)rsp >= lo && (char *)rsp < hi);

        printf("[signal stack]\n");
        printf("  altstack            = %p .. %p\n", (void *)lo, (void *)hi);
        printf("  rsp in handler      = 0x%lx\n", rsp);
        printf("  running on altstack = %s\n", on ? "YES" : "NO");
        printf("  rsp & ~0xffff       = 0x%lx  <- what Wine calls the TEB\n",
               rsp & ~0xfffful);
        if (!on)
            printf("  VERDICT: SA_ONSTACK not honoured - Wine's TEB lookup is"
                   " reading garbage.\n");
        else
            printf("  VERDICT: altstack works.\n");
        fflush(stdout);

        report("inside SIGINT handler");
    }

    static void *worker(void *arg)
    {
        (void)arg;
        report("spawned thread");
        return NULL;
    }

    int main(void)
    {
        pthread_t t;

        report("main thread");
        if (pthread_create(&t, NULL, worker, NULL) == 0)
            pthread_join(t, NULL);

        /* Same setup Wine uses: an altstack, and SA_ONSTACK on the handler. */
        {
            struct sigaction sa;
            g_altstack.ss_size = SIGSTKSZ * 4;
            g_altstack.ss_sp = malloc(g_altstack.ss_size);
            g_altstack.ss_flags = 0;
            if (sigaltstack(&g_altstack, NULL) != 0)
                printf("sigaltstack() FAILED: %s\n", strerror(errno));

            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = handler;
            sa.sa_flags = SA_ONSTACK;
            sigemptyset(&sa.sa_mask);
            if (sigaction(SIGINT, &sa, NULL) != 0)
                printf("sigaction() FAILED: %s\n", strerror(errno));
        }
        printf("\nraising SIGINT...\n");
        fflush(stdout);
        raise(SIGINT);

        report("after handler returned");
        return 0;
    }
    EOF

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -I${libSystem}/usr/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains \
      -lSystem -o gsbase-test gsbase-test.c
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/bin
    cp gsbase-test $out/usr/bin/
    runHook postInstall
  '';

  dontFixup = true;
  meta = with lib; {
    description = "Reports whether thread_info(THREAD_IDENTIFIER_INFO) returns a usable thread_handle (/usr/bin/gsbase-test)";
    platforms = platforms.unix;
  };
}
