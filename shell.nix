{ nixpkgs ? import <nixpkgs> {} }:
(import ./default.nix { inherit nixpkgs; }).overrideAttrs (oldAttrs: {
   buildInputs = oldAttrs.buildInputs ++ [ nixpkgs.valgrind ];
})