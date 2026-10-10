#!/bin/bash
set -e

# MinGW-w64 build for th06util.exe
python3 scripts/build.py all -j"$(nproc)" "$@"

if [ -d "$HOME/Desktop/th/kouma" ]; then
    cp build/th06util.exe "$HOME/Desktop/th/kouma/"
    cp build/th06_spell_practice.dll "$HOME/Desktop/th/kouma/"
    cp build/launcher.exe "$HOME/Desktop/th/kouma/"
fi
