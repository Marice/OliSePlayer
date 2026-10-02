#!/usr/bin/env bash
# Build libxmp-lite (MOD/XM/S3M/IT player) for the PS5 payload build and for
# the desktop test build. Installs into deps/ps5 and deps/native (gitignored).
#
#   PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk tools/build-deps.sh
#
# Re-running is safe; existing builds are replaced.
set -euo pipefail

LIBXMP_VERSION=4.7.3
LIBXMP_URL="https://github.com/libxmp/libxmp/releases/download/libxmp-${LIBXMP_VERSION}/libxmp-lite-${LIBXMP_VERSION}.tar.gz"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${ROOT}/deps/src"
SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"

mkdir -p "${WORK}"
cd "${WORK}"
if [ ! -d "libxmp-lite-${LIBXMP_VERSION}" ]; then
    echo "==> downloading libxmp-lite ${LIBXMP_VERSION}"
    curl -sL -o "libxmp-lite-${LIBXMP_VERSION}.tar.gz" "${LIBXMP_URL}"
    tar xzf "libxmp-lite-${LIBXMP_VERSION}.tar.gz"
fi
SRC="${WORK}/libxmp-lite-${LIBXMP_VERSION}"
CMAKE_OPTS=(-DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC=ON -DBUILD_SHARED=OFF)

if [ -x "${SDK}/bin/prospero-cmake" ]; then
    echo "==> building libxmp-lite for PS5 -> deps/ps5"
    rm -rf "${WORK}/build-ps5"
    PS5_PAYLOAD_SDK="${SDK}" "${SDK}/bin/prospero-cmake" -S "${SRC}" -B "${WORK}/build-ps5" \
        "${CMAKE_OPTS[@]}" -DCMAKE_INSTALL_PREFIX="${ROOT}/deps/ps5" > /dev/null
    make -C "${WORK}/build-ps5" -j"$(nproc)" > /dev/null
    make -C "${WORK}/build-ps5" install > /dev/null
else
    echo "!! PS5 SDK not found at ${SDK}; skipping the PS5 build"
fi

if command -v cmake > /dev/null; then
    echo "==> building libxmp-lite natively -> deps/native"
    rm -rf "${WORK}/build-native"
    cmake -S "${SRC}" -B "${WORK}/build-native" "${CMAKE_OPTS[@]}" \
        -DCMAKE_INSTALL_PREFIX="${ROOT}/deps/native" > /dev/null
    make -C "${WORK}/build-native" -j"$(nproc)" > /dev/null
    make -C "${WORK}/build-native" install > /dev/null
fi

echo "done:"
ls -1 "${ROOT}"/deps/*/lib/libxmp-lite.a
