#!/bin/sh
# vntest: vulkaninfo and the offscreen triangle on venus, then the triangle on every other icd for comparison
PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin; export PATH
icds=/usr/share/vulkan/icd.d
ls $icds
venus=$(ls $icds/virtio_icd*.json 2>/dev/null | head -1)
echo "vntest: venus icd $venus"

VK_DRIVER_FILES=$venus VN_DEBUG=init PD_DRM_DEBUG=1 vulkaninfo --summary 2>&1
for d in 0 1 2; do
	echo "vntest: venus device $d"
	PDVKTRI_DEVICE=$d VK_DRIVER_FILES=$venus pdvktri copy /var/tmp/vn-venus$d.ppm 2>&1
	PDVKTRI_DEVICE=$d VK_DRIVER_FILES=$venus pdvktri linear /var/tmp/vn-venus$d-linear.ppm 2>&1
done
for icd in $icds/*.json; do
	[ "$icd" = "$venus" ] && continue
	echo "vntest: reference $icd"
	VK_DRIVER_FILES=$icd pdvktri copy /var/tmp/vn-ref.ppm 2>&1
done

# the virgl gl path shares IOVirtIOGPU: renderer string and a few seconds of gears. xvfb is built without
# glx, so this needs the real server on the virtio-gpu framebuffer
if command -v Xorg >/dev/null 2>&1; then
	Xorg :7 -nolisten tcp >/var/tmp/xorg7.log 2>&1 &
	xpid=$!
	sleep 8
	DISPLAY=:7 glxinfo -B 2>&1 | grep -iE "renderer|version|error"
	DISPLAY=:7 glxgears >/var/tmp/gears.log 2>&1 &
	gpid=$!
	sleep 15
	kill $gpid $xpid 2>/dev/null
	cat /var/tmp/gears.log
	grep -iE "\(EE\)|glx" /var/tmp/xorg7.log | head -8
fi
