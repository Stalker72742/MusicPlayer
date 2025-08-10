#!/bin/bash
set -e

# Настрой путь к NDK и версию API
export ANDROID_NDK_HOME=/opt/android-sdk/ndk/27.2.12479018
export API=34
TOOLCHAIN=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64

PREFIX=$(pwd)/build/arm64-v8a

./configure \
  --prefix=$PREFIX \
  --target-os=android \
  --arch=aarch64 \
  --cpu=armv8-a \
  --cross-prefix=$TOOLCHAIN/bin/aarch64-linux-android- \
  --cc=$TOOLCHAIN/bin/aarch64-linux-android$API-clang \
  --cxx=$TOOLCHAIN/bin/aarch64-linux-android$API-clang++ \
  --strip=$TOOLCHAIN/bin/llvm-strip \
  --enable-cross-compile \
  --enable-shared \
  --disable-static \
  --disable-doc \
  --disable-programs \
  --disable-avdevice \
  --disable-postproc \
  --disable-symver \
  --enable-small \
  --disable-x86asm

make clean
make -j$(nproc)
make install
