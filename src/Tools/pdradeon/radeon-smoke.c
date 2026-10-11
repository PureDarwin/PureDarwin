#include <IOKit/IOKitLib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "PDRadeonRenderShared.h"
#define CHECK(expr) do { kern_return_t kr = (expr); if (kr) { printf("radeon-smoke: %s failed 0x%x\n", #expr, kr); return 1; } } while (0)
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    io_iterator_t iter;
    CHECK(IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("PDRadeonFramebuffer"), &iter));
    io_service_t svc; io_connect_t conn = 0;
    while ((svc = IOIteratorNext(iter))) {
        kern_return_t kr = IOServiceOpen(svc, mach_task_self(), PD_RADEON_RENDER_CLIENT, &conn);
        IOObjectRelease(svc);
        if (!kr) break;
        conn = 0;
        printf("radeon-smoke: open failed 0x%x\n", kr);
    }
    IOObjectRelease(iter);
    if (!conn) { puts("radeon-smoke: no accelerator"); return 1; }
    struct pd_radeon_info info = {0}; size_t size = sizeof(info);
    CHECK(IOConnectCallStructMethod(conn, PD_RADEON_INFO, NULL, 0, &info, &size));
    printf("radeon-smoke: PCI %04x heap %llu MB SIMD %u shader %08x tiling %08x\n", info.pci_id, (unsigned long long)(info.heap_size >> 20), info.active_simds, info.shader_config, info.tiling_config);
    uint64_t in[2] = {4096,4096}, out[3]; uint32_t n = 3;
    CHECK(IOConnectCallScalarMethod(conn, PD_RADEON_ALLOC, in, 2, out, &n));
    mach_vm_address_t addr = 0; mach_vm_size_t len = 0;
    CHECK(IOConnectMapMemory64(conn, out[0], mach_task_self(), &addr, &len, kIOMapAnywhere));
    volatile uint32_t *bo = (volatile uint32_t *)(uintptr_t)addr;
    printf("radeon-smoke: handle %llu MC %08llx mapping %p initial %08x\n", (unsigned long long)out[0], (unsigned long long)out[1], (void *)(uintptr_t)addr, bo[0]);
    uint32_t commands[] = {0xc0033d00u, (uint32_t)out[1], (1u<<18), 0x12345678, 0,
        0xc0033d00u, (uint32_t)out[1]+4, (1u<<18), 0x90abcdef, 0,
        0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000};
    CHECK(IOConnectCallStructMethod(conn, PD_RADEON_SUBMIT, commands, sizeof(commands), NULL, NULL));
    printf("radeon-smoke: GPU wrote %08x %08x\n", bo[0], bo[1]);
    int good = bo[0] == 0x12345678 && bo[1] == 0x90abcdef;
    // gpu copy of a vram region into ordinary memory
    uint64_t big[2] = {4u << 20, 4096}, bigOut[3]; uint32_t bigN = 3;
    CHECK(IOConnectCallScalarMethod(conn, PD_RADEON_ALLOC, big, 2, bigOut, &bigN));
    mach_vm_address_t baddr = 0; mach_vm_size_t blen = 0;
    CHECK(IOConnectMapMemory64(conn, bigOut[0], mach_task_self(), &baddr, &blen, kIOMapAnywhere));
    volatile uint32_t *bb = (volatile uint32_t *)(uintptr_t)baddr;
    for (unsigned i = 0; i < (1u << 20); i++) bb[i] = i * 2654435761u;
    uint8_t *dst = malloc((4u << 20) + 4096);
    uint8_t *aligned = dst + 100;
    memset(dst, 0xee, (4u << 20) + 4096);
    uint64_t cin[7] = {bigOut[0], 0, 7680, 7680, 546, 7680, (uint64_t)(uintptr_t)aligned};
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    kern_return_t ckr = IOConnectCallScalarMethod(conn, PD_RADEON_COPY_OUT, cin, 7, NULL, NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    unsigned bad = 0, total = 7680 * 546 / 4;
    for (unsigned i = 0; i < total; i++) if (((uint32_t *)aligned)[i] != i * 2654435761u) bad++;
    printf("radeon-smoke: copy-out kr 0x%x %u of %u words wrong, %.2f ms for %u KB\n", ckr, bad, total,
        (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6, total * 4 / 1024);
    good = good && !ckr && !bad;
    CHECK(IOConnectUnmapMemory64(conn, bigOut[0], mach_task_self(), baddr));
    CHECK(IOConnectCallScalarMethod(conn, PD_RADEON_FREE, bigOut, 1, NULL, NULL));
    free(dst);
    CHECK(IOConnectUnmapMemory64(conn, out[0], mach_task_self(), addr));
    CHECK(IOConnectCallScalarMethod(conn, PD_RADEON_FREE, out, 1, NULL, NULL));
    CHECK(IOServiceClose(conn));
    puts(good ? "radeon-smoke: PASS" : "radeon-smoke: FAIL");
    return !good;
}
