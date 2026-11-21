{
  description = "INKSTORM development tools for a host GCC and raylib";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { nixpkgs, ... }: {
    devShells = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ] (system:
      let pkgs = nixpkgs.legacyPackages.${system};
      in {
        default = pkgs.mkShellNoCC {
          packages = with pkgs; [
            cmake
            ninja
            clang-tools
            cppcheck
            glslang
            gdb
            ccache
            zip
            unzip
            python3
            shellcheck
          ];

          shellHook = ''
            if [[ ! -v RAYLIB_DIR ]]; then
              export RAYLIB_DIR="$HOME/.local/opt/raylib-6.2"
            fi
            if [[ -e /dev/dxg && ! -v GALLIUM_DRIVER ]]; then
              export GALLIUM_DRIVER=d3d12
            fi
          '';
        };
      });
  };
}
