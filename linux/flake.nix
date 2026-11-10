# Argon, PureDarwin's desktop, on Linux: the packages, a NixOS module and a VM that boots into it
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      # the PureDarwin tree this flake sits in, which nix copies whole as the flake lives in its git repository
      argon-src = ../.;
      argonPackages.${system} = import ./nix/packages.nix {
        inherit pkgs;
        argonSrc = argon-src;
        # the build number in SystemVersion.plist
        revision = self.shortRev or self.dirtyShortRev or "dirty";
      };
      vm = headless: (nixpkgs.lib.nixosSystem {
        inherit system;
        modules = [
          self.nixosModules.argon
          "${nixpkgs}/nixos/modules/virtualisation/qemu-vm.nix"
          (import ./nix/vm.nix { inherit headless; })
        ];
      });
      # each run gets its own directory for the disk, sockets and serial log, printed first
      runner = name: config: pkgs.writeShellScript name ''
        dir=''${ARGON_VM_DIR:-$(mktemp -d /tmp/${name}.XXXXXX)}
        mkdir -p "$dir" && cd "$dir" || exit 1
        echo "${name}: $dir"
        # the VM script runs qemu inside TMPDIR, so the sockets in the qemu options land in $dir
        export NIX_DISK_IMAGE=$dir/disk.qcow2 TMPDIR=$dir USE_TMPDIR=1
        exec ${config.config.system.build.vm}/bin/run-argon-vm "$@"
      '';
      helper = name: file: {
        type = "app";
        program = "${pkgs.writeShellScript name ''
          export PATH=${pkgs.lib.makeBinPath [ pkgs.socat pkgs.coreutils ]}:$PATH
          exec sh ${file} "$@"
        ''}";
      };
    in
    {
      packages.${system} = argonPackages.${system} // { default = argonPackages.${system}.argon; };

      nixosModules.argon = import ./nix/module.nix { inherit argonPackages; };

      nixosConfigurations.argon-vm = vm false;
      nixosConfigurations.argon-vm-headless = vm true;

      apps.${system} = {
        argon-vm = { type = "app"; program = "${runner "argon-vm" self.nixosConfigurations.argon-vm}"; };
        argon-vm-headless = { type = "app"; program = "${runner "argon-vm-headless" self.nixosConfigurations.argon-vm-headless}"; };
        # screenshots, clicks and serial commands for a headless VM, ARGON_VM_DIR names its directory
        vm-shot = helper "vm-shot" ./vm-shot.sh;
        vm-click = helper "vm-click" ./vm-click.sh;
        vm-run = helper "vm-run" ./vm-run.sh;
      };
    };
}
