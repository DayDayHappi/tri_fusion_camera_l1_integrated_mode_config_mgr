set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ================================
# Lindi RK3588J SDK paths
# ================================
set(LINDI_SDK_ROOT "/home/alientek/neardi/rk3588/neardi-3588-linux-5.10")
set(LINDI_BUILDROOT_OUTPUT "${LINDI_SDK_ROOT}/buildroot/output/rockchip_rk3588")
set(LINDI_HOST "${LINDI_BUILDROOT_OUTPUT}/host")
set(LINDI_SYSROOT "${LINDI_HOST}/aarch64-buildroot-linux-gnu/sysroot")

# ================================
# Compilers
# Use Buildroot toolchain wrapper
# ================================
set(CMAKE_C_COMPILER   "${LINDI_HOST}/bin/aarch64-buildroot-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${LINDI_HOST}/bin/aarch64-buildroot-linux-gnu-g++")

# ================================
# Sysroot
# ================================
set(CMAKE_SYSROOT "${LINDI_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH "${LINDI_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ================================
# pkg-config
# Force CMake/pkg-config to use Lindi sysroot
# ================================
set(PKG_CONFIG_EXECUTABLE "${LINDI_HOST}/bin/pkg-config" CACHE FILEPATH "pkg-config")

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${LINDI_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}
    "${LINDI_SYSROOT}/usr/lib/pkgconfig:${LINDI_SYSROOT}/usr/share/pkgconfig:${LINDI_SYSROOT}/lib/pkgconfig")

# ================================
# Runtime linker search policy
# Do not use host Ubuntu library paths
# ================================
set(CMAKE_SKIP_RPATH TRUE)

# ================================
# C++ standard behavior
# ================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
