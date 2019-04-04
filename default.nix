{ nixpkgs ? import <nixpkgs> {} }:
(nixpkgs.callPackage ./triangle.nix {}).overrideAttrs (oldAttrs: {
   shellHook = ''
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${nixpkgs.vulkan-validation-layers}/lib
export XDG_DATA_DIRS=$XDG_DATA_DIRS:${nixpkgs.vulkan-validation-layers}/share
'';
})
