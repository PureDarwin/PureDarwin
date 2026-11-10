#!/bin/sh
# vm-click.sh X Y [X2 Y2]: a left click in the headless VM at screen pixel (X, Y), or a drag from there to (X2, Y2),
# through the USB tablet, monitor mouse_move only sends relative motion
base=${ARGON_VM_DIR:?set ARGON_VM_DIR to the directory nix run .#argon-vm-headless printed}
w=${ARGON_VM_WIDTH:-1280} h=${ARGON_VM_HEIGHT:-800}
abs() { printf '{"execute":"input-send-event","arguments":{"events":[{"type":"abs","data":{"axis":"x","value":%d}},{"type":"abs","data":{"axis":"y","value":%d}}]}}\n' $(( $1 * 32767 / (w - 1) )) $(( $2 * 32767 / (h - 1) )); }
btn() { printf '{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"button":"left","down":%s}}]}}\n' "$1"; }
{
  echo '{"execute":"qmp_capabilities"}'
  abs "$1" "$2"; sleep 0.2
  btn true; sleep 0.15
  if [ -n "$4" ]; then
    # a few steps on the way, so the compositor sees motion with the button held
    for i in 1 2 3 4 5 6 7 8; do abs $(( $1 + ($3 - $1) * i / 8 )) $(( $2 + ($4 - $2) * i / 8 )); sleep 0.05; done
    sleep 0.15
  fi
  btn false; sleep 0.2
} | socat - "unix:$base/vm-qmp" > /dev/null
