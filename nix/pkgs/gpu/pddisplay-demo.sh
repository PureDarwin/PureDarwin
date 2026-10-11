#!/bin/sh
# scanout demo: the cube through VK_KHR_display at three sizes and vkcube on the display wsi, then
# Xorg scanning out of its own buffer with vkcube over the x11 software wsi, fps goes to the console
sleep ${PDDISPLAY_DELAY:-20}
S=${PDDISPLAY_SECONDS:-15}
echo "pddisplay: phase display"
for size in "256 256" "800 600" ""; do
	echo "pddisplay: mark display ${size:-full}"
	/usr/local/bin/pdvkdisplay $S $size 2>&1 | grep -v "mvm_protect"
done
t0=$(date +%s)
/usr/bin/vkcube --wsi display --c 600 2>&1 | grep -v "mvm_protect"
t1=$(date +%s)
echo "pddisplay: vkcube --wsi display 600 frames in $((t1 - t0)) s"

echo "pddisplay: phase x11"
mkdir -p /var/log /tmp/.X11-unix /usr/share/X11/xkb/compiled
rm -f /tmp/.X11-unix/X0 /tmp/.X0-lock
/usr/bin/Xorg :0 -config /etc/X11/xorg.conf -logfile /var/log/Xorg.0.log -nolisten tcp -s 0 -dpms &
n=0
while [ ! -S /tmp/.X11-unix/X0 ] && [ $n -lt 60 ]; do
	sleep 1
	n=$((n + 1))
done
sleep 3
grep -a "scanout\|shadow copy" /var/log/Xorg.0.log
export DISPLAY=:0 MESA_VK_WSI_DEBUG=sw
/usr/bin/xeyes -geometry 240x240+1600+60 &
for size in "256 256" "800 600" "1920 1080"; do
	set -- $size
	echo "pddisplay: mark x11 ${1}x${2}"
	t0=$(date +%s)
	/usr/bin/vkcube --c 300 --width $1 --height $2 2>&1 | grep -v "mvm_protect"
	t1=$(date +%s)
	echo "pddisplay: x11 vkcube ${1}x${2} 300 frames in $((t1 - t0)) s"
done
echo "pddisplay: mark x11 hold"
exec /usr/bin/vkcube --width 800 --height 600
