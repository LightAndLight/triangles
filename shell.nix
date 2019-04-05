{ nixpkgs ? import <nixos-unstable> {} }:
(import ./default.nix { inherit nixpkgs; }).overrideAttrs (oldAttrs: {
   buildInputs = oldAttrs.buildInputs ++ [ nixpkgs.gdb nixpkgs.valgrind ];
})
