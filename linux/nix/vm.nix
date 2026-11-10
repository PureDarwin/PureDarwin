# vm.nix: a NixOS VM that boots straight into Argon. headless leaves the screen to QMP screenshots and clicks,
# otherwise the VM opens a GTK window. the disk and sockets live in the directory it is started from
{ headless ? false }:
{ lib, pkgs, ... }:

{
  system.stateVersion = "26.05";
  networking.hostName = "argon";

  users.users.argon = {
    isNormalUser = true;
    extraGroups = [ "video" "input" "render" "seat" ];
    password = "argon";
  };

  services.argon = {
    enable = true;
    autologin = { enable = true; user = "argon"; };
  };

  # a root shell on the serial port, which vm-run.sh types into
  services.getty.autologinUser = "root";
  # asargon runs a command inside the argon user's session, for tests over the serial shell
  environment.systemPackages = [ pkgs.foot pkgs.fastfetch pkgs.gdb pkgs.valgrind pkgs.psmisc (pkgs.writeShellScriptBin "asargon" ''
    p=$(pgrep -u argon -x MenuBar || pgrep -u argon -x Dock | head -n 1)
    exec runuser -u argon -- env $(tr '\0' '\n' < /proc/$p/environ | grep -E '^(WAYLAND_DISPLAY|XDG_RUNTIME_DIR|ARGON_ROOT|FONTCONFIG_FILE|LD_LIBRARY_PATH|HOME|PATH|ARGON_LOG_DIR)=') "$@"
  '') ];
  # crashes keep a core for gdb
  systemd.coredump.enable = true;

  virtualisation = {
    memorySize = 4096;
    cores = 4;
    graphics = true;
    qemu.options = [ "-vga none" "-device virtio-vga" ] ++ (if headless then [
      "-display none"
      "-monitor unix:vm-mon,server,nowait"
      "-qmp unix:vm-qmp,server,nowait"
      "-chardev socket,id=ser0,path=vm-serial,server=on,wait=off,logfile=vm-serial.log"
      "-serial chardev:ser0"
    ] else [
      "-display gtk,zoom-to-fit=off"
    ]);
  };
}
