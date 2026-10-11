#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>

#include "v10_structs.h"

// gc 10.3 compute queue registers, all segment 0, from the amdgpu headers (MIT)
#define mmGRBM_GFX_CNTL                 0x0dc2
#define mmCP_MEC_CNTL                   0x0f55
#define mmCP_MEC_DOORBELL_RANGE_LOWER   0x1dfc
#define mmCP_MEC_DOORBELL_RANGE_UPPER   0x1dfd
#define mmCP_PQ_WPTR_POLL_CNTL          0x1e23
#define mmCP_PQ_STATUS                  0x1e58
#define mmCP_MQD_BASE_ADDR              0x1fa9
#define mmCP_MQD_BASE_ADDR_HI           0x1faa
#define mmCP_HQD_ACTIVE                 0x1fab
#define mmCP_HQD_VMID                   0x1fac
#define mmCP_HQD_PERSISTENT_STATE       0x1fad
#define mmCP_HQD_QUANTUM                0x1fb0
#define mmCP_HQD_PQ_BASE                0x1fb1
#define mmCP_HQD_PQ_BASE_HI             0x1fb2
#define mmCP_HQD_PQ_RPTR                0x1fb3
#define mmCP_HQD_PQ_RPTR_REPORT_ADDR    0x1fb4
#define mmCP_HQD_PQ_RPTR_REPORT_ADDR_HI 0x1fb5
#define mmCP_HQD_PQ_WPTR_POLL_ADDR      0x1fb6
#define mmCP_HQD_PQ_WPTR_POLL_ADDR_HI   0x1fb7
#define mmCP_HQD_PQ_DOORBELL_CONTROL    0x1fb8
#define mmCP_HQD_PQ_CONTROL             0x1fba
#define mmCP_HQD_IB_CONTROL             0x1fbe
#define mmCP_HQD_DEQUEUE_REQUEST        0x1fc1
#define mmCP_MQD_CONTROL                0x1fcb
#define mmCP_HQD_EOP_BASE_ADDR          0x1fce
#define mmCP_HQD_EOP_BASE_ADDR_HI       0x1fcf
#define mmCP_HQD_EOP_CONTROL            0x1fd0
#define mmCP_HQD_PQ_WPTR_LO             0x1fdf
#define mmCP_HQD_PQ_WPTR_HI             0x1fe0
#define mmSCRATCH_REG0                  0x2040      // segment 1
#define mmRLC_CP_SCHEDULERS             0x4ca1      // segment 1

// nvd.h
#define PACKET3(op, n)              ((3u << 30) | (((op) & 0xff) << 8) | (((n) & 0x3fff) << 16))
#define PACKET3_SET_UCONFIG_REG     0x79
#define PACKET3_SET_UCONFIG_START   0x0000c000
#define PACKET3_SET_RESOURCES       0xa0
#define PACKET3_MAP_QUEUES          0xa2

// the kiq lives on mec 2 pipe 1 queue 0 with doorbell 0, as amdgpu places it on this chip
#define kKiqMe          2
#define kKiqPipe        1
#define kKiqQueue       0
#define kKiqDoorbell    0
#define kUserQueueEnd   0x8a
#define kGfxDoorbell    (0x8b << 1)

// carveout layout from kiqBase: ring, rptr writeback, wptr shadow, mqd, eop
#define kKiqRingSize    0x1000
#define kKiqRptrOff     0x1000
#define kKiqWptrOff     0x1010
#define kKiqMqdOff      0x2000
#define kKiqEopOff      0x4000
#define kMecHpdSize     2048

void PDAmdGPU::grbmSelect(uint32_t me, uint32_t pipe, uint32_t queue)
{
    wreg(soc15(kHwGC, 0, mmGRBM_GFX_CNTL), (pipe & 0x3) | ((me & 0x3) << 2) | ((queue & 0x7) << 8));
}

static uint32_t log2u(uint32_t v)
{
    uint32_t n = 0;
    while ((1u << n) < v) n++;
    return n;
}

// gfx_v10_0_kiq_init_queue and kcq_resume, then the kiq maps the gfx queue the way enable_kgq does
bool PDAmdGPU::kiqMapGfx(uint64_t gfxMqdMc, uint64_t gfxWptrMc)
{
    auto gc = [this](uint32_t off) { return soc15(kHwGC, 0, off); };
    uint64_t base = pspBase + 0x3010000;
    uint64_t ringMc = fbStart + base;
    uint64_t rptrMc = ringMc + kKiqRptrOff;
    uint64_t wptrMc = ringMc + kKiqWptrOff;
    uint64_t mqdMc = ringMc + kKiqMqdOff;
    uint64_t eopMc = ringMc + kKiqEopOff;

    writeVram(base, 0, kKiqEopOff + kMecHpdSize);

    // gfx_v10_0_kiq_setting
    uint32_t sched = (rreg(soc15(kHwGC, 1, mmRLC_CP_SCHEDULERS)) & ~0xffu) | (kKiqMe << 5) | (kKiqPipe << 3) | kKiqQueue;
    wreg(soc15(kHwGC, 1, mmRLC_CP_SCHEDULERS), sched);
    wreg(soc15(kHwGC, 1, mmRLC_CP_SCHEDULERS), sched | 0x80);

    grbmSelect(kKiqMe, kKiqPipe, kKiqQueue);

    // gfx_v10_0_compute_mqd_init
    v10_compute_mqd *mqd = (v10_compute_mqd *)IOMallocZeroData(sizeof(v10_compute_mqd));
    if (!mqd) {
        grbmSelect(0, 0, 0);
        return false;
    }
    mqd->header = 0xc0310800;
    mqd->compute_pipelinestat_enable = 1;
    mqd->compute_static_thread_mgmt_se0 = 0xffffffff;
    mqd->compute_static_thread_mgmt_se1 = 0xffffffff;
    mqd->compute_static_thread_mgmt_se2 = 0xffffffff;
    mqd->compute_static_thread_mgmt_se3 = 0xffffffff;
    mqd->compute_misc_reserved = 3;
    mqd->cp_hqd_eop_base_addr_lo = (uint32_t)(eopMc >> 8);
    mqd->cp_hqd_eop_base_addr_hi = (uint32_t)(eopMc >> 40);
    mqd->cp_hqd_eop_control = (rreg(gc(mmCP_HQD_EOP_CONTROL)) & ~0x3fu) | (log2u(kMecHpdSize / 4) - 1);
    mqd->cp_hqd_pq_doorbell_control = (rreg(gc(mmCP_HQD_PQ_DOORBELL_CONTROL)) & ~0xdffffffcu) | (kKiqDoorbell << 2) | 0x40000000;
    mqd->cp_mqd_base_addr_lo = (uint32_t)mqdMc & ~3u;
    mqd->cp_mqd_base_addr_hi = (uint32_t)(mqdMc >> 32);
    mqd->cp_mqd_control = rreg(gc(mmCP_MQD_CONTROL)) & ~0xfu;
    mqd->cp_hqd_pq_base_lo = (uint32_t)(ringMc >> 8);
    mqd->cp_hqd_pq_base_hi = (uint32_t)(ringMc >> 40);
    // queue size and rptr block size in log2 dwords minus one, unordered dispatch, privileged, kernel queue
    uint32_t pq = rreg(gc(mmCP_HQD_PQ_CONTROL)) & ~(0x3fu | 0x3f00u | 0xf0000000u);
    mqd->cp_hqd_pq_control = pq | (log2u(kKiqRingSize / 4) - 1) | ((log2u(4096 / 4) - 1) << 8) | 0x10000000 | 0x40000000 | 0x80000000;
    mqd->cp_hqd_pq_rptr_report_addr_lo = (uint32_t)rptrMc & ~3u;
    mqd->cp_hqd_pq_rptr_report_addr_hi = (uint32_t)(rptrMc >> 32) & 0xffff;
    mqd->cp_hqd_pq_wptr_poll_addr_lo = (uint32_t)wptrMc & ~3u;
    mqd->cp_hqd_pq_wptr_poll_addr_hi = (uint32_t)(wptrMc >> 32) & 0xffff;
    mqd->cp_hqd_pq_rptr = rreg(gc(mmCP_HQD_PQ_RPTR));
    mqd->cp_hqd_vmid = 0;
    mqd->cp_hqd_persistent_state = (rreg(gc(mmCP_HQD_PERSISTENT_STATE)) & ~0x3ff00u) | (0x53 << 8);
    mqd->cp_hqd_ib_control = (rreg(gc(mmCP_HQD_IB_CONTROL)) & ~0x300000u) | (3 << 20);
    mqd->cp_hqd_quantum = 0x1 | 0x10 | (1 << 8);
    mqd->cp_hqd_active = 1;
    copyToVram(base + kKiqMqdOff, mqd, sizeof(v10_compute_mqd));
    flushHdp();

    // gfx_v10_0_kiq_init_register
    wreg(gc(mmCP_PQ_WPTR_POLL_CNTL), rreg(gc(mmCP_PQ_WPTR_POLL_CNTL)) & ~0x80000000u);
    if (rreg(gc(mmCP_HQD_ACTIVE)) & 1) {
        wreg(gc(mmCP_HQD_DEQUEUE_REQUEST), 1);
        for (int i = 0; i < 1000 && (rreg(gc(mmCP_HQD_ACTIVE)) & 1); i++)
            IODelay(1);
        wreg(gc(mmCP_HQD_DEQUEUE_REQUEST), 0);
        wreg(gc(mmCP_HQD_PQ_RPTR), mqd->cp_hqd_pq_rptr);
        wreg(gc(mmCP_HQD_PQ_WPTR_LO), 0);
        wreg(gc(mmCP_HQD_PQ_WPTR_HI), 0);
    }
    wreg(gc(mmCP_HQD_PQ_DOORBELL_CONTROL), 0);
    wreg(gc(mmCP_HQD_EOP_BASE_ADDR), mqd->cp_hqd_eop_base_addr_lo);
    wreg(gc(mmCP_HQD_EOP_BASE_ADDR_HI), mqd->cp_hqd_eop_base_addr_hi);
    wreg(gc(mmCP_HQD_EOP_CONTROL), mqd->cp_hqd_eop_control);
    wreg(gc(mmCP_MQD_BASE_ADDR), mqd->cp_mqd_base_addr_lo);
    wreg(gc(mmCP_MQD_BASE_ADDR_HI), mqd->cp_mqd_base_addr_hi);
    wreg(gc(mmCP_MQD_CONTROL), mqd->cp_mqd_control);
    wreg(gc(mmCP_HQD_PQ_BASE), mqd->cp_hqd_pq_base_lo);
    wreg(gc(mmCP_HQD_PQ_BASE_HI), mqd->cp_hqd_pq_base_hi);
    wreg(gc(mmCP_HQD_PQ_CONTROL), mqd->cp_hqd_pq_control);
    wreg(gc(mmCP_HQD_PQ_RPTR_REPORT_ADDR), mqd->cp_hqd_pq_rptr_report_addr_lo);
    wreg(gc(mmCP_HQD_PQ_RPTR_REPORT_ADDR_HI), mqd->cp_hqd_pq_rptr_report_addr_hi);
    wreg(gc(mmCP_HQD_PQ_WPTR_POLL_ADDR), mqd->cp_hqd_pq_wptr_poll_addr_lo);
    wreg(gc(mmCP_HQD_PQ_WPTR_POLL_ADDR_HI), mqd->cp_hqd_pq_wptr_poll_addr_hi);
    wreg(gc(mmCP_MEC_DOORBELL_RANGE_LOWER), (kKiqDoorbell * 2) << 2);
    wreg(gc(mmCP_MEC_DOORBELL_RANGE_UPPER), (kUserQueueEnd * 2) << 2);
    wreg(gc(mmCP_HQD_PQ_DOORBELL_CONTROL), mqd->cp_hqd_pq_doorbell_control);
    wreg(gc(mmCP_HQD_PQ_WPTR_LO), 0);
    wreg(gc(mmCP_HQD_PQ_WPTR_HI), 0);
    wreg(gc(mmCP_HQD_VMID), 0);
    wreg(gc(mmCP_HQD_PERSISTENT_STATE), mqd->cp_hqd_persistent_state);
    wreg(gc(mmCP_HQD_ACTIVE), 1);
    wreg(gc(mmCP_PQ_STATUS), rreg(gc(mmCP_PQ_STATUS)) | 0x2);
    IOFreeData(mqd, sizeof(v10_compute_mqd));

    grbmSelect(0, 0, 0);

    // gfx_v10_0_cp_compute_enable
    wreg(gc(mmCP_MEC_CNTL), 0);
    IODelay(50);

    // set_resources (kcq_resume), map_queues for the gfx queue (enable_kgq), then the kiq ring test's scratch write
    uint32_t scratch = soc15(kHwGC, 1, mmSCRATCH_REG0);
    wreg(scratch, 0xcafedead);
    uint32_t pkt[32];
    uint32_t n = 0;
    pkt[n++] = PACKET3(PACKET3_SET_RESOURCES, 6);
    pkt[n++] = 0;                       // vmid mask 0, queue type 0 (kiq)
    pkt[n++] = 0;                       // queue mask, no compute queues yet
    pkt[n++] = 0;
    pkt[n++] = 0;                       // cleaner shader
    pkt[n++] = 0;
    pkt[n++] = 0;                       // oac mask
    pkt[n++] = 0;                       // gds heap
    pkt[n++] = PACKET3(PACKET3_MAP_QUEUES, 5);
    // queue 0 pipe 0, me field 1 for a gfx ring, engine select 4 (gfx), one queue
    pkt[n++] = (0 << 13) | (0 << 16) | (1 << 18) | (4 << 26) | (1u << 29);
    pkt[n++] = kGfxDoorbell << 2;
    pkt[n++] = (uint32_t)gfxMqdMc;
    pkt[n++] = (uint32_t)(gfxMqdMc >> 32);
    pkt[n++] = (uint32_t)gfxWptrMc;
    pkt[n++] = (uint32_t)(gfxWptrMc >> 32);
    pkt[n++] = PACKET3(PACKET3_SET_UCONFIG_REG, 1);
    pkt[n++] = scratch - PACKET3_SET_UCONFIG_START;
    pkt[n++] = 0x1234abcd;
    copyToVram(base, pkt, n * 4);
    uint32_t shadow[2] = { n, 0 };
    copyToVram(base + kKiqWptrOff, shadow, sizeof(shadow));
    flushHdp();
    ringDoorbell(kKiqDoorbell, n);

    uint32_t v = 0;
    for (int i = 0; i < 1000 && (v = rreg(scratch)) != 0x1234abcd; i++)
        IODelay(100);

    grbmSelect(kKiqMe, kKiqPipe, kKiqQueue);
    uint32_t hqdRptr = rreg(gc(mmCP_HQD_PQ_RPTR)), hqdActive = rreg(gc(mmCP_HQD_ACTIVE));
    grbmSelect(0, 0, 0);
    LOG("kiq: ring test %s, SCRATCH_REG0 0x%08x, kiq rptr %u of %u active %u, RLC_CP_SCHEDULERS 0x%08x CP_MEC_CNTL 0x%08x\n",
        v == 0x1234abcd ? "passed" : "failed", v, hqdRptr, n, hqdActive, rreg(soc15(kHwGC, 1, mmRLC_CP_SCHEDULERS)),
        rreg(gc(mmCP_MEC_CNTL)));
    if (v != 0x1234abcd) dumpGfxState("kiq ring test");
    return v == 0x1234abcd;
}
