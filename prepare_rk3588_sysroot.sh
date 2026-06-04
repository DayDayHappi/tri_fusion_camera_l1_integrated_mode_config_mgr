#!/usr/bin/env bash
set -e

SDK_ROOT="/home/alientek/tronlong/RK3588/rk3588_linux_release"
SYSROOT="$HOME/rk3588_sysroot"
BOARD_IP="192.168.1.21"

RGA_INC_DIR="${SDK_ROOT}/external/linux-rga/im2d_api"
MPP_INC_DIR="${SDK_ROOT}/external/mpp/inc"

echo "[INFO] SDK_ROOT=${SDK_ROOT}"
echo "[INFO] SYSROOT=${SYSROOT}"
echo "[INFO] BOARD_IP=${BOARD_IP}"

if [ ! -f "${RGA_INC_DIR}/im2d.h" ]; then
    echo "[ERROR] im2d.h not found: ${RGA_INC_DIR}/im2d.h"
    exit 1
fi

if [ ! -f "${MPP_INC_DIR}/rk_mpi.h" ]; then
    echo "[ERROR] rk_mpi.h not found: ${MPP_INC_DIR}/rk_mpi.h"
    exit 1
fi

rm -rf "${SYSROOT}"
mkdir -p "${SYSROOT}/usr/include/rga"
mkdir -p "${SYSROOT}/usr/include/rockchip"
mkdir -p "${SYSROOT}/usr/lib"

echo "[COPY] RGA headers"
cp -r "${RGA_INC_DIR}/"* "${SYSROOT}/usr/include/rga/"

echo "[COPY] MPP headers"
cp -r "${MPP_INC_DIR}/"* "${SYSROOT}/usr/include/rockchip/"

echo "[COPY] RGA / MPP runtime libraries from board"
scp root@${BOARD_IP}:/usr/lib/librga.so* "${SYSROOT}/usr/lib/"
scp root@${BOARD_IP}:/usr/lib/librockchip_mpp.so* "${SYSROOT}/usr/lib/"

echo
echo "[CHECK] sysroot:"
find "${SYSROOT}" -name "im2d.h"
find "${SYSROOT}" -name "rk_mpi.h"
find "${SYSROOT}" -name "mpp_buffer.h"
find "${SYSROOT}" -name "mpp_frame.h"
find "${SYSROOT}" -name "mpp_packet.h"
find "${SYSROOT}" -name "librga.so*"
find "${SYSROOT}" -name "librockchip_mpp.so*"

echo
echo "[DONE] RK3588 sysroot prepared."
