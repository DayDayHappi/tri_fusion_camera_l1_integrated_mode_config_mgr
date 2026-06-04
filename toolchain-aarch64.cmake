set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)

set(CMAKE_SYSROOT "$ENV{HOME}/rk3588_sysroot")

set(CMAKE_FIND_ROOT_PATH
    ${CMAKE_SYSROOT}
    /usr/aarch64-linux-gnu
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS "--sysroot=${CMAKE_SYSROOT} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "--sysroot=${CMAKE_SYSROOT} ${CMAKE_CXX_FLAGS}")

# 说明：
# librga.so / librockchip_mpp.so 是从板子拷过来的运行库，
# 它们依赖板子自己的 glibc / libstdc++。
# 当前 Ubuntu 交叉工具链 sysroot 版本较旧，链接阶段不要强制解析这些 .so 的内部依赖。
set(CMAKE_EXE_LINKER_FLAGS
    "--sysroot=${CMAKE_SYSROOT} -L${CMAKE_SYSROOT}/usr/lib -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib -Wl,--allow-shlib-undefined ${CMAKE_EXE_LINKER_FLAGS}"
)

set(CMAKE_SHARED_LINKER_FLAGS
    "--sysroot=${CMAKE_SYSROOT} -L${CMAKE_SYSROOT}/usr/lib -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib -Wl,--allow-shlib-undefined ${CMAKE_SHARED_LINKER_FLAGS}"
)
