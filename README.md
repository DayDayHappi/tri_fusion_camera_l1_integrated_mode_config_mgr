# Tri-Fusion Camera L1 Hardware Access Layer

本工程是在已给出的 L0 `tri_fusion_foundation` 基础上实现的 L1 Hardware Access Layer。

## 分层边界

L1 只封装 Linux/RK3588J 底层资源：V4L2、UVC、串口、MPP、RGA、RKNN、基础 socket。L1 不出现 WorkMode、ModeManager、GB28181、ONVIF、Private API、MainStream 等上层概念。

## 与 L0 的依赖关系

- `tri_fusion_hardware` 链接 `tri_fusion_foundation`
- 所有接口返回 `tri::foundation::Result<T>` 或 `Result<void>`
- 错误码使用 `tri::foundation::ErrorCode`
- 日志使用 `TRI_LOG_*` 和 `LogCategory::{V4L2,Uvc,Serial,System}`
- 串口配置可直接从 `tri::foundation::SerialConfig` 转换
- 相机配置可直接从 `tri::foundation::CameraEndpointConfig` 转换

## 编译

```bash
cmake -S . -B build
cmake --build build -j
./build/l1_smoke_test
```

## 复合相机采集验证

```bash
./build/l1_probe ./configs /dev/video0 800 600 30
```

如果不传节点参数，工具会优先读取 `configs/camera.yaml` 中的 `camera.composite_low_thermal.video_node`。

## 串口验证

```bash
./build/l1_serial_probe ./configs
```

该工具只打开配置中的串口，不发送业务模式命令。业务模式命令应在 L3 `CompositeSensorController` 实现。
