#!/bin/sh
# vm-run.sh COMMAND: type a command at the headless VM's serial root shell, output lands in vm-serial.log
base=${ARGON_VM_DIR:?set ARGON_VM_DIR to the directory nix run .#argon-vm-headless printed}
printf '%s\n' "$*" | socat - "unix:$base/vm-serial" > /dev/null
