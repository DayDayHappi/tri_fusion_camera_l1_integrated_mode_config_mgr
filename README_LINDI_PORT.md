# 林滴 RK3588J 移植编译说明

本文档用于记录三光融合相机工程从创龙 RK3588J 移植到林滴 RK3588J 后的独立编译、打包和运行方式。

## 1. 目录约定

林滴与创龙构建产物必须分开，禁止混用。

```text
cmake/toolchains/toolchain-lindi-rk3588.cmake    # 林滴专用 toolchain
scripts/build_lindi.sh                           # 林滴专用编译脚本
build-lindi/                                     # 林滴独立 CMake 构建目录
package/lindi/bin/                               # 林滴可执行文件输出目录
package/lindi/configs/                           # 林滴配置文件输出目录
package/lindi/lib/                               # 林滴库输出目录
```

创龙版本继续使用独立目录，例如：

```text
build-chuanglong/
package/chuanglong/
```

## 2. 林滴 SDK 路径

当前林滴 SDK 路径：

```bash
/home/alientek/neardi/rk3588/neardi-3588-linux-5.10
```

林滴 sysroot：

```bash
/home/alientek/neardi/rk3588/neardi-3588-linux-5.10/buildroot/output/rockchip_rk3588/host/aarch64-buildroot-linux-gnu/sysroot
```

林滴交叉编译器：

```bash
/home/alientek/neardi/rk3588/neardi-3588-linux-5.10/buildroot/output/rockchip_rk3588/host/bin/aarch64-buildroot-linux-gnu-gcc
/home/alientek/neardi/rk3588/neardi-3588-linux-5.10/buildroot/output/rockchip_rk3588/host/bin/aarch64-buildroot-linux-gnu-g++
```

## 3. 编译命令

在工程根目录执行：

```bash
cd ~/PRJ/tri_fusion_camera_l1_integrated_mode_config_mgr
./scripts/build_lindi.sh
```

脚本会执行以下动作：

```text
1. 使用林滴 SDK toolchain
2. 使用林滴 SDK sysroot
3. 清理主机环境变量污染
4. 只构建 l8_private_gstreamer_control_probe 目标
5. 输出到 package/lindi/bin/
6. 同步 configs 到 package/lindi/configs/
```

## 4. 输出文件

编译成功后生成：

```bash
package/lindi/bin/l8_private_gstreamer_control_probe
package/lindi/configs/
```

检查文件类型：

```bash
file package/lindi/bin/l8_private_gstreamer_control_probe
```

检查动态依赖：

```bash
readelf -d package/lindi/bin/l8_private_gstreamer_control_probe | grep NEEDED
```

检查 ABI 版本：

```bash
readelf --version-info package/lindi/bin/l8_private_gstreamer_control_probe \
  | grep -E 'GLIBC_|GLIBCXX_' \
  | sort -u
```

当前林滴版已确认最高依赖约为：

```text
GLIBC_2.34
GLIBCXX_3.4.29
```

不再依赖创龙版本中的：

```text
GLIBC_2.38
GLIBCXX_3.4.30
```

## 5. 当前直接依赖库

林滴版可执行文件当前依赖：

```text
libgstapp-1.0.so.0
libgstvideo-1.0.so.0
libgstbase-1.0.so.0
libgstreamer-1.0.so.0
libgobject-2.0.so.0
libglib-2.0.so.0
librga.so.2
libOpenCL.so.1
libmali.so.1
librockchip_mpp.so.1
libstdc++.so.6
libm.so.6
libgcc_s.so.1
libc.so.6
ld-linux-aarch64.so.1
```

## 6. 部署到林滴板

在虚拟机执行：

```bash
cd ~/PRJ/tri_fusion_camera_l1_integrated_mode_config_mgr

scp package/lindi/bin/l8_private_gstreamer_control_probe root@林滴板IP:/root/Chuanglongyizhi/
scp -r package/lindi/configs root@林滴板IP:/root/Chuanglongyizhi/
```

## 7. 林滴板运行前检查

在林滴板执行：

```bash
cd /root/Chuanglongyizhi
chmod +x ./l8_private_gstreamer_control_probe
ldd ./l8_private_gstreamer_control_probe
```

确认没有：

```text
not found
```

检查设备节点：

```bash
ls -l /dev/video* 2>/dev/null
ls -l /dev/tri_composite_serial /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
ls -l /dev/rga /dev/mpp_service /dev/dri/* 2>/dev/null
```

检查 GStreamer 插件：

```bash
gst-inspect-1.0 v4l2src
gst-inspect-1.0 appsink
gst-inspect-1.0 appsrc
gst-inspect-1.0 mpph264enc
gst-inspect-1.0 mppjpegdec
gst-inspect-1.0 h264parse
gst-inspect-1.0 mpegtsmux
gst-inspect-1.0 udpsink
```

## 8. 启动命令

```bash
cd /root/Chuanglongyizhi

./l8_private_gstreamer_control_probe \
  --config-dir ./configs \
  --host 192.168.1.153 \
  --port 5004 \
  --control-port 18080 \
  --serial-dev /dev/tri_composite_serial
```

如果 `/dev/tri_composite_serial` 不存在，先临时替换为实际串口，例如：

```bash
--serial-dev /dev/ttyACM0
```

## 9. 后台运行并保存日志

```bash
cd /root/Chuanglongyizhi

./l8_private_gstreamer_control_probe \
  --config-dir ./configs \
  --host 192.168.1.153 \
  --port 5004 \
  --control-port 18080 \
  --serial-dev /dev/tri_composite_serial \
  > /tmp/l8_lindi_run.log 2>&1 &

sleep 5
cat /tmp/l8_lindi_run.log
```

## 10. HTTP 控制测试

```bash
curl http://127.0.0.1:18080/api/v1/status
curl -X POST http://127.0.0.1:18080/api/v1/mode/visible
curl -X POST http://127.0.0.1:18080/api/v1/mode/lowlight_thermal
curl -X POST http://127.0.0.1:18080/api/v1/mode/visible_composite
```

## 11. 注意事项

1. 不要把创龙板的可执行文件直接拷到林滴板运行。
2. 不要把创龙板的 `libc.so.6`、`libstdc++.so.6`、`ld-linux-aarch64.so.1` 拷到林滴板覆盖系统库。
3. 林滴必须使用林滴 SDK 重新编译。
4. 林滴和创龙的 build、package、toolchain 文件必须独立维护。
5. 若运行时报缺库，优先检查林滴 rootfs 是否包含 GStreamer、RGA、MPP、OpenCL、Mali 相关库。
