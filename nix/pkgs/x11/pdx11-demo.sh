#!/bin/sh
# waits for Xorg :0, then shows xeyes and times vkcube over the software x11 wsi
n=0
while [ ! -S /tmp/.X11-unix/X0 ] && [ $n -lt 300 ]; do
	sleep 1
	n=$((n + 1))
done
[ -S /tmp/.X11-unix/X0 ] || { echo "pdx11: no X server after ${n}s"; tail -n 40 /var/log/Xorg.0.log; exit 1; }
# the server makes its socket before the screen is up, so give it a moment
sleep 3
if ! ps ax | grep -q "[X]org"; then
	echo "pdx11: Xorg died"
	grep -a "(EE)" /var/log/Xorg.0.log | head -n 5
	# no scanout (hdmi unplugged at boot): exercise the clients on Xvfb instead
	echo "pdx11: falling back to Xvfb, nothing will reach the screen"
	rm -f /tmp/.X11-unix/X0 /tmp/.X0-lock
	/usr/bin/Xvfb :0 -screen 0 1920x1080x24 -nolisten tcp &
	n=0
	while [ ! -S /tmp/.X11-unix/X0 ] && [ $n -lt 60 ]; do
		sleep 1
		n=$((n + 1))
	done
	sleep 2
fi
echo "pdx11: X server up after ${n}s"

/usr/bin/xeyes -geometry 240x240+1600+60 &
echo "pdx11: xeyes pid $!"
sleep 2

frames=${PDX11_FRAMES:-600}
t0=$(date +%s)
/usr/bin/vkcube --c "$frames" --width 800 --height 600
rc=$?
t1=$(date +%s)
dt=$((t1 - t0))
[ $dt -gt 0 ] || dt=1
echo "pdx11: vkcube rc $rc, $frames frames in ${dt}s, $((frames / dt)) fps (incl. startup)"

# leave a cube on screen for the capture
exec /usr/bin/vkcube --width 800 --height 600
