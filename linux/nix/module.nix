# module.nix: services.argon, PureDarwin's Argon desktop as a NixOS session
{ argonPackages }:
{ config, lib, pkgs, ... }:

let
  cfg = config.services.argon;
  root = "/var/lib/argon";
  session = pkgs.writeShellScript "argon-session-env" ''
    export ARGON_ROOT=${root}
    export ARGON_APPS=${lib.escapeShellArg (lib.concatStringsSep " " cfg.apps)}
    export ARGON_LOG_DIR=''${XDG_RUNTIME_DIR:-/tmp}/argon
    exec ${cfg.package}/bin/argon-session
  '';
  desktopFile = pkgs.writeTextDir "share/wayland-sessions/argon.desktop" ''
    [Desktop Entry]
    Name=Argon
    Comment=PureDarwin's desktop on neuswc
    Exec=${session}
    Type=Application
    DesktopNames=Argon
  '';
in
{
  options.services.argon = {
    enable = lib.mkEnableOption "Argon, PureDarwin's desktop";

    package = lib.mkOption {
      type = lib.types.package;
      default = argonPackages.${pkgs.stdenv.hostPlatform.system}.argon;
      description = "The Argon tree: frameworks, apps, compositor and fonts.";
    };

    apps = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ "Applications/Utilities/Terminal" ];
      example = [ "System/Library/CoreServices/Finder" "Applications/Calculator" "Applications/Utilities/Terminal" ];
      description = "Apps opened when the session starts, as bundle paths under the Argon root without .app.";
    };

    autologin = {
      enable = lib.mkEnableOption "starting the session for a user at boot, without a greeter";
      user = lib.mkOption {
        type = lib.types.str;
        description = "The user whose session starts at boot.";
      };
    };
  };

  config = lib.mkIf cfg.enable {
    # swc-launch takes the VT and hands neuswc DRM and input devices, as the setuid launcher it was written to be
    security.wrappers.swc-launch = {
      source = "${cfg.package}/bin/swc-launch";
      owner = "root";
      group = "root";
      setuid = true;
    };

    # the frameworks read /System and /Applications under ARGON_ROOT and write preferences into its Library
    systemd.tmpfiles.rules = [
      "d ${root} 0755 root root -"
      "L+ ${root}/System - - - - ${cfg.package}/System"
      "L+ ${root}/Applications - - - - ${cfg.package}/Applications"
      "L+ ${root}/bin - - - - ${cfg.package}/bin"
      "L+ ${root}/lib - - - - ${cfg.package}/lib"
      "L+ ${root}/etc - - - - ${cfg.package}/etc"
      "d ${root}/Library 0755 root root -"
      "d ${root}/Library/Preferences 1777 root root -"
    ];

    environment.systemPackages = [ cfg.package desktopFile ];
    environment.pathsToLink = [ "/share/wayland-sessions" ];
    environment.sessionVariables = {
      ARGON_ROOT = root;
      FONTCONFIG_FILE = "${cfg.package}/etc/fonts/argon.conf";
    };
    services.displayManager.sessionPackages = [ (desktopFile.overrideAttrs (_: { passthru.providedSessions = [ "argon" ]; })) ];

    fonts.packages = [ argonPackages.${pkgs.stdenv.hostPlatform.system}.fonts ];
    hardware.graphics.enable = true;
    services.seatd.enable = lib.mkDefault true;

    # greetd starts the session on tty1, straight in with autologin, behind a text greeter otherwise
    services.greetd = {
      enable = true;
      settings = {
        default_session.command = "${pkgs.tuigreet}/bin/tuigreet --time --sessions /run/current-system/sw/share/wayland-sessions";
      } // lib.optionalAttrs cfg.autologin.enable {
        initial_session = { command = "${session}"; user = cfg.autologin.user; };
      };
    };
  };
}
