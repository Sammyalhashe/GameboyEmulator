{
  description = "NESEmulator build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          name = "NESEmulator";
          src = ./.;

          nativeBuildInputs = [ pkgs.cmake ];
          buildInputs = [ ];

          installPhase = ''
            mkdir -p $out/bin
            cp NESEmulator $out/bin/
          '';
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            gcc
            gdb
          ];
        };
      }
    );
}
