let pkgs = import <nixos-unstable> {}; in
import ./default.nix { nixpkgs = pkgs; }
