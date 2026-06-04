# Tri-Fusion Camera L2 Device Abstraction Layer

本目录是对 `src/device/` 的 L2 设备抽象层实现，可直接复制到已完成 L0/L1 的 `tri_fusion_camera_l1_integrated` 工程中。

## 实现边界

- L2 只抽象设备对象：`CameraDevice`、`VisibleCameraDevice`、`CompositeLowThermalCameraDevice`、`CameraManager`。
- L2 调用已有 L1 接口：`V4L2Device`、`UvcDevice`、`UvcMetadataReader`、`makeCaptureFormat`。
- L2 不实现工作模式切换，不调用串口模式命令，不出现 GB28181 / ONVIF / RTSP / MainStream / ModeManager。
- `/dev/video1` 只作为 UVC metadata 辅助节点读取；metadata 打不开不影响 `/dev/video0` 主视频采集。

## 合入方式

```bash
cp -r src/device /path/to/tri_fusion_camera_l1_integrated/src/
cp tests/l2_device_smoke_test.cpp /path/to/tri_fusion_camera_l1_integrated/tests/
cd /path/to/tri_fusion_camera_l1_integrated
patch -p1 < /path/to/tri_fusion_camera_l2_device_layer/patches/CMakeLists_l2.patch
cmake -S . -B build
cmake --build build -j
./build/l2_device_smoke_test
```

## 上板采集调用方式示例

```cpp
tri::device::CameraManager manager;
manager.init(tri::foundation::ConfigManager::instance().camera());
manager.openAllEnabled();
manager.startCamera(tri::device::CameraId::CompositeLowThermal);
auto frame = manager.compositeCamera()->readFrame(2000);
```

该示例只启动复合相机当前已经处于的输出模式；切换 LOWLIGHT / THERMAL / COMPOSITE 必须放到下一层 `device_control/CompositeSensorController`。
