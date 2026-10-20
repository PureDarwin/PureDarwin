Adreno clean-room driver for PureDarwin


What this is

This project writes an Adreno GPU kernel driver for PureDarwin, which runs on the XNU kernel. The driver is licensed MIT or BSD so it can live in the PureDarwin tree, and it is written without using the GPL Linux code (drivers/gpu/drm/msm and the drivers around it). The first target is the Adreno 306 (A306) in the MSM8916 chip, as found in the UZ801 USB stick. The design is kept general so it can grow to the rest of the A3xx to A5xx family and to other Qualcomm chips.


Roles

There are two roles, and they are kept apart.

1. The observer, who also writes the specs. The observer runs the GPL Linux driver as a black box on real hardware and records what the hardware sees: register state, register write sequences (from tracepoints), ring and command stream contents, and clock and power domain state. The observer reads only the allowed sources listed below, and writes specs in spec/. A spec describes behaviour and values, never code.
2. The implementer. The implementer writes the PureDarwin driver from spec/, the allowed sources and the hardware itself. The implementer does not look at the raw dumps in observations/ when a spec already covers the topic, and never looks at the forbidden sources.

On this project the observer and spec writer was a language model, which is allowed. It ran the Linux driver as test equipment, recorded what the hardware did and wrote the specs here. I implemented the driver myself, working from those specs, the allowed sources and the hardware.


Allowed sources

- Mesa (MIT): src/freedreno/**, which includes the register database in registers/adreno/*.xml, the msm and kgsl userspace backends in drm/, computerator, and the documentation for the decode tools cffdump, crashdec and afuc. Also Mesa's include/drm-uapi/msm_drm.h, the msm user interface header, which is MIT licensed.
- The envytools rnndb (MIT), meaning the freedreno register documentation that ships with Mesa.
- LK and lk2nd (MIT and BSD-3, from CodeAurora), for the MSM8916 clock, regulator and SMD/RPM code. SMD and RPM are the channels used to talk to the chip's resource power manager.
- FreeBSD drivers (BSD-2) and OpenBSD drivers (ISC).
- ARM's public specifications: the SMMU architecture, the GIC and the Arm Architecture Reference Manual.
- Observations of how the hardware behaves under the Linux driver, kept in observations/.


Forbidden sources

- Linux kernel GPL code, mainline or downstream. That means drivers/gpu/drm/msm, drivers/clk/qcom, drivers/pmdomain/qcom (the GDSC power switches), drivers/iommu/arm/arm-smmu (qcom_iommu.c and arm-smmu-qcom), drivers/soc/qcom, drivers/interconnect/qcom, and the downstream kgsl driver.
- The Linux tree in the observation kernel's build directory is compiled and run as test equipment only. Nobody on this project reads its driver sources.

One caveat. The language model that wrote the specs has seen Linux kernel source during training. The process therefore keeps every concrete detail (offsets, values and sequences) traceable to an allowed source or to an observation recorded here, and that is what makes the result defensible. Every spec entry cites where its information came from.


Layout

- spec/ holds behaviour specs written from the observations and the allowed sources, with citations.
- spec/observations/ holds the user interface parameter tables the specs cite.

The raw captures from the stick (register dumps, traces and rd captures, which are recordings of submitted command streams) and the observation tooling (the gltri workload, a slimmed down freedreno Mesa build, regdump and msmparams) stay in the MSM8916 work tree outside PureDarwin. References to observations/run1/ and tools/ in the specs point there.


In the PureDarwin tree

This directory holds the clean-room specs that the PDDRMmsm kernel extension is written from (a306-msm8916-bringup.md and msm-uapi.md, plus a690-sc8280xp-bringup.md, the spec for the a6xx generation on the sc8280xp, checked against observations of a ThinkPad X13s in its version 1 but not yet implemented), and the user interface parameter tables they cite (in observations/). The a690 raw captures, like the MSM8916 ones, stay outside the tree. Its GPU state dumps contain SQE microcode, which was removed before anything read them (a690 spec, section 7.1). The raw register dumps, the traces and the GPU microcode stay out of the tree. The microcode is loaded from /Library/Firmware/qcom the first time the device is opened.


Adding a generation or a chip

The kernel extension is split into what every Adreno shares and what one generation does:

- pd_msm_gpu.c holds the shared parts: register mapping, the IOMMU (chosen by the compatible string of the node that the GPU's iommus property names), clocks (the chip's core clock at its top rate), the start done at first open, submit locking and fence waiting.
- struct pd_msm_gpu_funcs, in pd_msm.h, holds the per generation operations: init, start, submit, completed and fence_ack. The a3xx code lives in pd_msm_a3xx.c. An a6xx would go in its own file with its own PKT4 and PKT7 ring code, its GMU bring-up and its microcode, exported the same way.
- struct pd_msm_info, in pd_msm_drv.c, holds what a chip reports to userspace (GPU_ID, CHIP_ID, GMEM and MAX_FREQ), its core clock name, its GPU virtual address range (64-bit GPU addresses are carried through the shared code) and its generation. Adding a chip means adding an entry there plus a compatible string in pd_msm_match.
- struct pd_msm_iommu describes an IOMMU type. The MSM8916's qcom,msm-iommu-v1 is the first one, in pd_msm_smmu.c. An arm,smmu-500 like the one on the sc8280xp would be another entry in the table in pd_msm_gpu.c.
- SoC clock and power providers register by compatible string. pd_msm_gcc8916.c is the one for msm8916, and another SoC adds its own file next to it.

Each new piece needs its own spec here first, written from the allowed sources and from observations, as described above.
