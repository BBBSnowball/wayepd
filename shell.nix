{ pkgs ? import <nixpkgs> {} }:
with pkgs;
mkShell {
    inputsFrom = [ wayvnc ];

    packages = [
        gdb
        jq
    ];

    shellHook = "";
}