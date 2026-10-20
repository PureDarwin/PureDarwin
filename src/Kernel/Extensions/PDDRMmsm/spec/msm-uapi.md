msm uAPI behaviour for A306 and A690, version 1


What this is

This spec describes what userspace (Mesa's freedreno driver) needs from a kernel driver that speaks the msm DRM user interface (uAPI), the set of ioctls the Linux msm driver offers. Version 0 covered the A306. Version 1 adds the A690 in the Snapdragon 8cx Gen 3 (sc8280xp), observed on a Lenovo ThinkPad X13s.

Every fact cites its source with one of these tags:

- [UAPI] is Mesa's include/drm-uapi/msm_drm.h (MIT).
- [MESA] is Mesa's src/freedreno/drm/msm/*.c (MIT), which shows how userspace calls each ioctl.
- [OBS] is observations/msm-uapi-params.txt, from the Linux driver queried as a black box with tools/msmparams.c on the stick.
- [OBS a690] is observations/a690-msm-uapi-params.txt, from the same kind of query against Linux 7.0.14 on the X13s (run1, 19 October 2026). See a690-sc8280xp-bringup.md, section 8.


Identity [OBS]

What the Linux driver answers to each query:

    DRM version                 1.10.0, name "msm", description "MSM Snapdragon DRM"
    GPU_ID                      307
    CHIP_ID                     0x03000600
    GMEM_SIZE                   0x20000 (128 KB)
    GMEM_BASE                   0x100000
    MAX_FREQ                    400000000
    PRIORITIES (= NR_RINGS)     3
    PP_PGTABLE                  0
    FAULTS                      0
    SUSPENDS                    1
    TIMESTAMP, VA_START, VA_SIZE, UCHE_TRAP_BASE, ...   EINVAL on this GPU

A PureDarwin driver may report version 1.5, which covers softpin and robustness [MESA FD_VERSION_*], and PRIORITIES 1, because Mesa clamps the queue priority to PRIORITIES - 1 [MESA msm_pipe.c].


Identity, A690 [OBS a690]

The same queries against the A690. Most of the parameters that return EINVAL on the A306 have values here.

    DRM version                 1.13.0, name "msm", description "MSM Snapdragon DRM"
    GPU_ID                      0 (the kernel reports none; see below)
    CHIP_ID                     0xffff06090000
    GMEM_SIZE                   0x400000 (4 MB)
    GMEM_BASE                   0
    MAX_FREQ                    690000000
    TIMESTAMP                   a counter value (0x1de3b when read), not EINVAL
    PRIORITIES (= NR_RINGS)     3
    PP_PGTABLE                  0
    FAULTS                      0
    SUSPENDS                    1
    VA_START                    0x100000000
    VA_SIZE                     0xffff00000000 (so the user range ends at 2^48)
    HIGHEST_BANK_BIT            16
    RAYTRACING                  0
    UBWC_SWIZZLE                6
    MACROTILE_MODE              1
    UCHE_TRAP_BASE              0x1fffffffff000
    HAS_PRR                     1
    SYSPROF, COMM, CMDLINE      EINVAL (write-only parameters)

GPU_ID 0 is deliberate on this chip: Mesa then matches the device by CHIP_ID. A CHIP_ID whose bits 47:32 are 0xffff matches the device table entry for FD690 whatever the fuse bits say, and Mesa derives the GPU id 690 from the core, major and minor bytes when it needs a number (a690-sc8280xp-bringup.md, section 1, from Mesa's device table). Under Linux, Mesa 26.0.8 reported the renderer "FD690" with these values. A PureDarwin driver for this chip should report the same GPU_ID and CHIP_ID rather than inventing 690.

Buffers on the A690: GEM_INFO with MSM_INFO_GET_IOVA handed out 0x100000000, 0x100001000 and 0x100003000 for 4, 8 and 16 KB buffers. These are the first addresses of the VA_START range, page granular. Submit queues at priorities 0, 1 and 2 are accepted (ids 1 to 3), and priority 3 returns EINVAL, which agrees with PRIORITIES 3.


Buffers [OBS, MESA]

- GEM_NEW sizes are page aligned. For this GPU, Mesa passes MSM_BO_WC (0x20000) in the flags.
- GEM_INFO with MSM_INFO_GET_IOVA returns a GPU virtual address. Linux handed out 0x01009000, 0x0100a000 and 0x0100c000 for 4, 8 and 16 KB buffers, so addresses are page granular and low in the 4 GB space.
- GEM_INFO with MSM_INFO_GET_OFFSET returns the mmap offset. On Linux that is 0x100000000 plus a page index.
- SET_NAME and GET_NAME carry a debug name. SET_METADATA and GET_METADATA may fail, and Mesa only warns when they do.


Submission [MESA msm_ringbuffer_sp.c]

GEM_SUBMIT is used with softpin (userspace picks the GPU addresses). Its fields are:

- flags is MSM_PIPE_3D0 (0x10), plus optional fence fd bits, and queueid names the queue.
- Each entry of bos[] is { flags READ/WRITE/DUMP, handle, presumed 0 }.
- Each entry of cmds[] is { type MSM_SUBMIT_CMD_BUF, submit_idx (an index into bos), submit_offset (in bytes), size (in bytes), nr_relocs 0 }.

The kernel executes each command buffer as an indirect buffer at the buffer's GPU address plus the offset, size/4 dwords long. It then signals a fence and returns its sequence number in req.fence. WAIT_FENCE waits for a sequence number on a queue. GEM_CPU_PREP waits for a buffer's last use, and with MSM_PREP_NOSYNC it reports busy instead of waiting.
