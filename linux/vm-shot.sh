#!/bin/sh
# vm-shot.sh NAME: screendump the headless Argon VM in $ARGON_VM_DIR to NAME.png there
base=${ARGON_VM_DIR:?set ARGON_VM_DIR to the directory nix run .#argon-vm-headless printed}
echo "screendump $base/$1.png -f png" | socat - "unix:$base/vm-mon" > /dev/null
sleep 0.5
echo "$base/$1.png"
