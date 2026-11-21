#!/bin/sh
# Builds the FFmpeg SoundLink uses into third_party/ffmpeg (git-ignored): shared, LGPL, audio only.
# The player links its DLLs; ffmpeg.exe is what yt-dlp runs to fix containers and write tags (-c copy, no encoders).
# The JPEG demuxer, parser and decoder let ffmpeg.exe put a cover into a downloaded m4a (also -c copy).
#
# Needs Git Bash (or MSYS2) and MinGW with mingw32-make on PATH, e.g. D:/Qt/Tools/mingw1310_64/bin. Takes a few minutes.
# Usage: tools/build-ffmpeg.sh [jobs]
#
# Never add --enable-gpl or --enable-nonfree: the application ships these DLLs.
set -eu

VERSION=9.0.2
SHA256=8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUTPUT="$ROOT/third_party/ffmpeg"
JOBS=${1:-$(nproc)}

# FFmpeg's configure refuses paths with spaces, and this repository may live in one.
WORK=${FFMPEG_BUILD_DIR:-${TEMP:-/tmp}/soundlink-ffmpeg}
WORK=$(cd "$(dirname "$WORK")" && pwd)/$(basename "$WORK")
case "$WORK" in
    *" "*) echo "The build directory must not contain spaces: $WORK (set FFMPEG_BUILD_DIR)" >&2; exit 1 ;;
esac

mkdir -p "$WORK/tmp"
# configure breaks on the backslashes of the Windows TEMP.
export TMPDIR="$WORK/tmp" TEMP="$WORK/tmp" TMP="$WORK/tmp"
cd "$WORK"

ARCHIVE="ffmpeg-$VERSION.tar.xz"
[ -f "$ARCHIVE" ] || curl -fsSL -o "$ARCHIVE" "https://ffmpeg.org/releases/$ARCHIVE"
echo "$SHA256  $ARCHIVE" | sha256sum -c -

rm -rf "ffmpeg-$VERSION" install
tar -xJf "$ARCHIVE"
cd "ffmpeg-$VERSION"

./configure \
    --prefix="$WORK/install" \
    --target-os=mingw32 --arch=x86_64 --cc=gcc --cxx=g++ \
    --enable-shared --disable-static \
    --disable-everything --disable-autodetect --disable-doc --disable-debug --disable-x86asm \
    --disable-avdevice --disable-swscale --disable-ffplay --disable-ffprobe \
    --extra-ldflags=-static-libgcc \
    --enable-network --enable-schannel \
    --enable-protocol=file,pipe,http,https,tcp,tls \
    --enable-demuxer=mov,matroska,ogg,mp3,flac,wav,asf,aac,image_jpeg_pipe \
    --enable-decoder=aac,aac_latm,alac,mp3,mp3float,opus,vorbis,flac,wmav1,wmav2,wmapro,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_u8,mjpeg \
    --enable-parser=aac,aac_latm,mpegaudio,opus,vorbis,flac,mjpeg \
    --enable-muxer=mp4,ipod,mov,webm,matroska,ogg,opus,mp3,flac,wav,adts,null \
    --enable-bsf=aac_adtstoasc \
    --enable-filter=aresample,aformat,anull,atrim,format,null

mingw32-make -j"$JOBS"
mingw32-make install

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT"
cp -r "$WORK/install/bin" "$WORK/install/include" "$WORK/install/lib" "$OUTPUT/"
cp COPYING.LGPLv2.1 "$OUTPUT/bin/FFmpeg-LICENSE.txt"
echo "$VERSION" > "$OUTPUT/VERSION"
strip "$OUTPUT"/bin/*.dll "$OUTPUT"/bin/ffmpeg.exe

du -sh "$OUTPUT/bin"
"$OUTPUT/bin/ffmpeg.exe" -hide_banner -version | sed -n 1,2p
echo "FFmpeg $VERSION is in $OUTPUT"
