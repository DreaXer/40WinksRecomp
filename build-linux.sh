#!/bin/sh
# Configure (first run) and build build-linux/40WinksRecompiled with clang, cmake and ninja.
# RecompiledFuncs/ and rsp/aspMain.cpp are not in git: copy them from the Windows checkout first.
# "./build-linux.sh package" also makes dist/40WinksRecompiled-<version>-linux.tar.gz.
set -e
cd "$(dirname "$0")"

if [ ! -f RecompiledFuncs/recomp_overlays.inl ] || [ ! -f rsp/aspMain.cpp ]; then
    echo "Missing RecompiledFuncs/ or rsp/aspMain.cpp: copy them from the Windows checkout." >&2
    exit 1
fi

if [ ! -f build-linux/build.ninja ]; then
    cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
fi
cmake --build build-linux --target 40WinksRecompiled

if [ "$1" = "package" ]; then
    version=$(sed -n 's/.*version_string = "\([^"]*\)".*/\1/p' src/main/main.cpp)
    name=40WinksRecompiled-v$version-linux
    rm -rf "dist/$name"
    mkdir -p "dist/$name"
    cp build-linux/40WinksRecompiled recompcontrollerdb.txt LICENSE release/README.txt release/LISEZMOI.txt "dist/$name/"
    # Same content as the Windows zip: assets/ without sounds/.
    cp -r assets "dist/$name/"
    rm -rf "dist/$name/assets/sounds"
    tar -czf "dist/$name.tar.gz" -C dist "$name"
    echo "dist/$name.tar.gz"
fi
