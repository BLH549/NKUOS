#!/usr/bin/env bash
# C / 2411238: build the source-level debugger outside the lab repository.
set -euo pipefail

task_root=${1:-"$HOME/.local/src"}
mkdir -p "$task_root"
task_root=$(cd "$task_root" && pwd)
task_archive="$task_root/qemu-4.1.1.tar.xz"
task_source="$task_root/qemu-4.1.1"
task_deps="$task_root/qemu-deps"

if [[ ! -f "$task_archive" ]]; then
    curl -L --fail --retry 2 https://download.qemu.org/qemu-4.1.1.tar.xz -o "$task_archive"
fi
sha256sum "$task_archive"
if [[ ! -f "$task_source/VERSION" ]]; then
    tar -xJf "$task_archive" -C "$task_root"
fi

# On this WSL installation only the pixman runtime was present. Extract its
# development package locally; apt download and dpkg-deb do not install it.
export PKG_CONFIG_LIBDIR=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig
if ! pkg-config --exists pixman-1; then
    mkdir -p "$task_deps/pkgconfig"
    (
        cd "$task_deps"
        if ! compgen -G 'libpixman-1-dev_*.deb' >/dev/null; then
            apt download libpixman-1-dev
        fi
        for task_package in libpixman-1-dev_*.deb; do
            dpkg-deb -x "$task_package" root
        done
    )
    sed "s|^prefix=/usr$|prefix=$task_deps/root/usr|" \
        "$task_deps/root/usr/lib/x86_64-linux-gnu/pkgconfig/pixman-1.pc" \
        > "$task_deps/pkgconfig/pixman-1.pc"
    export PKG_CONFIG_PATH="$task_deps/pkgconfig"
fi

mkdir -p "$task_source/build-debug"
cd "$task_source/build-debug"
../configure --target-list=riscv64-softmmu --enable-debug \
    --disable-werror --disable-docs --disable-gtk --disable-sdl \
    --disable-vnc --disable-tools --disable-curses --disable-curl \
    --python=/usr/bin/python3
make -j"${JOBS:-12}"
./riscv64-softmmu/qemu-system-riscv64 --version
printf '\nDebug binary: %s\n' "$task_source/build-debug/riscv64-softmmu/qemu-system-riscv64"
