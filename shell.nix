{ pkgs ? import <nixpkgs> {} }:
with pkgs;
mkShell {
    inputsFrom = [ wayvnc ];

    packages = [
        gdb
        jq
        #imagemagick_light
        (imagemagick_light.overrideAttrs (old: { dontStrip = true; }))
    ];

    shellHook = "";
}