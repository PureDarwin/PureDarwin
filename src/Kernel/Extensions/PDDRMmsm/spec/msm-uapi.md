msm uAPI behaviour for A306, version 0


What this is

This spec describes what userspace (Mesa's freedreno driver) needs from a kernel driver that speaks the msm DRM user interface (uAPI), the set of ioctls the Linux msm driver offers.

Every fact cites its source with one of these tags:

- [UAPI] is Mesa's include/drm-uapi/msm_drm.h (MIT).
- [MESA] is Mesa's src/freedreno/drm/msm/*.c (MIT), which shows how userspace calls each ioctl.
- [OBS] is observations/msm-uapi-params.txt, from the Linux driver queried as a black box with tools/msmparams.c on the stick.


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
