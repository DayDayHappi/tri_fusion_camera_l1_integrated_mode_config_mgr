#!/usr/bin/env bash
set -euo pipefail

PROJ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

LINDI_SDK_ROOT="/home/alientek/neardi/rk3588/neardi-3588-linux-5.10"
LINDI_BUILDROOT_OUTPUT="${LINDI_SDK_ROOT}/buildroot/output/rockchip_rk3588"
LINDI_HOST="${LINDI_BUILDROOT_OUTPUT}/host"
LINDI_SYSROOT="${LINDI_HOST}/aarch64-buildroot-linux-gnu/sysroot"

BUILD_DIR="${PROJ_ROOT}/build-lindi"
PKG_DIR="${PROJ_ROOT}/package/lindi"
BIN_DIR="${PKG_DIR}/bin"
CFG_DIR="${PKG_DIR}/configs"

TOOLCHAIN_FILE="${PROJ_ROOT}/cmake/toolchains/toolchain-lindi-rk3588.cmake"

export PATH="${LINDI_HOST}/bin:${PATH}"
export PKG_CONFIG_SYSROOT_DIR="${LINDI_SYSROOT}"
export PKG_CONFIG_LIBDIR="${LINDI_SYSROOT}/usr/lib/pkgconfig:${LINDI_SYSROOT}/usr/share/pkgconfig:${LINDI_SYSROOT}/lib/pkgconfig"

echo "========== Lindi build environment =========="
echo "PROJ_ROOT        = ${PROJ_ROOT}"
echo "LINDI_SDK_ROOT   = ${LINDI_SDK_ROOT}"
echo "LINDI_HOST       = ${LINDI_HOST}"
echo "LINDI_SYSROOT    = ${LINDI_SYSROOT}"
echo "BUILD_DIR        = ${BUILD_DIR}"
echo "BIN_DIR          = ${BIN_DIR}"
echo "TOOLCHAIN_FILE   = ${TOOLCHAIN_FILE}"
echo

echo "========== Compiler check =========="
which aarch64-buildroot-linux-gnu-gcc
which aarch64-buildroot-linux-gnu-g++
aarch64-buildroot-linux-gnu-gcc --version | head -1
aarch64-buildroot-linux-gnu-g++ --version | head -1
echo

echo "========== pkg-config check =========="
pkg-config --modversion gstreamer-1.0
pkg-config --libs gstreamer-1.0
pkg-config --cflags gstreamer-1.0
echo

mkdir -p "${BUILD_DIR}"
mkdir -p "${BIN_DIR}"
mkdir -p "${CFG_DIR}"

cmake -S "${PROJ_ROOT}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DTRI_TARGET_BOARD=lindi \
  -DTRI_FUSION_TARGET_SYSROOT="${LINDI_SYSROOT}" \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="${BIN_DIR}" \
  -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="${PKG_DIR}/lib" \
  -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY="${PKG_DIR}/lib"

cmake --build "${BUILD_DIR}" --target l8_private_gstreamer_control_probe -j"$(nproc)"

echo
echo "========== Copy configs =========="
rsync -a --delete "${PROJ_ROOT}/configs/" "${CFG_DIR}/"

echo
echo "========== Find executable =========="
EXE_PATH="$(find "${BIN_DIR}" "${BUILD_DIR}" -type f -name "l8_private_gstreamer_control_probe" | head -n 1 || true)"

if [ -z "${EXE_PATH}" ]; then
  echo "[ERROR] l8_private_gstreamer_control_probe not found"
  echo "Please check CMake target output."
  exit 1
fi

if [ "${EXE_PATH}" != "${BIN_DIR}/l8_private_gstreamer_control_probe" ]; then
  cp -f "${EXE_PATH}" "${BIN_DIR}/l8_private_gstreamer_control_probe"
fi

chmod +x "${BIN_DIR}/l8_private_gstreamer_control_probe"

echo
echo "========== Output =========="
ls -lh "${BIN_DIR}/l8_private_gstreamer_control_probe"

echo
echo "========== ABI version requirement =========="
readelf --version-info "${BIN_DIR}/l8_private_gstreamer_control_probe" \
  | grep -E 'GLIBC_|GLIBCXX_' \
  | sort -u || true

echo
echo "========== Direct dependencies =========="
readelf -d "${BIN_DIR}/l8_private_gstreamer_control_probe" | grep NEEDED || true

echo
echo "[OK] Lindi build finished."
echo "Package directory: ${PKG_DIR}"
