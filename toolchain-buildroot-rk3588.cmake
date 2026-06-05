set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(BUILDROOT_HOST
    /home/alientek/tronlong/RK3588/rk3588_linux_release/buildroot/output/rockchip_rk3588/host
)

set(CMAKE_SYSROOT
    ${BUILDROOT_HOST}/aarch64-buildroot-linux-gnu/sysroot
)

set(CMAKE_C_COMPILER
    ${BUILDROOT_HOST}/bin/aarch64-buildroot-linux-gnu-gcc
)

set(CMAKE_CXX_COMPILER
    ${BUILDROOT_HOST}/bin/aarch64-buildroot-linux-gnu-g++
)

set(CMAKE_FIND_ROOT_PATH
    ${CMAKE_SYSROOT}
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
