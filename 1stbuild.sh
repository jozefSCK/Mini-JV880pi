#!/bin/bash
set -ex

git submodule update --init --recursive
sh submod.sh

if [ ! -f gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf.tar.xz ]; then
    wget -q https://developer.arm.com/-/media/Files/downloads/gnu-a/10.3-2021.07/binrel/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf.tar.xz
fi

tar xf gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf.tar.xz

export PATH=$(readlink -f ./gcc-*aarch64-none*/bin/):$PATH

RPI=3 bash -ex build.sh