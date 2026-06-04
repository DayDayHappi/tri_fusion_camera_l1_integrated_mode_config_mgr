# L0 System Foundation Layer 设计说明

## 设计边界

L0 是全系统基础库，只向上提供能力，不向上层反向依赖。此层不得出现：

- WorkMode、CompositeSensorOutputMode 等模式业务类型
- V4L2、UVC、SerialPort、MPP、RGA 的底层实现
- GB28181、ONVIF、RTSP、Private API 协议实现
- MediaPipeline、CameraManager、ModeManager 等上层对象

## 当前实现

- `Result<T>` + `ErrorCode`：所有模块统一错误返回模型
- `Logger`：统一日志入口，支持类别化输出
- `ConfigManager`：加载 camera/serial/mode/media/protocol 配置，并输出强类型配置对象
- `EventBus`：跨模块事件通知，仅用于状态变化通知，不承载复杂业务流程
- `ThreadPool` / `Worker` / `BlockingQueue`：提供上层线程模型基础
- `Clock` / `Timer` / `Timestamp`：统一时间戳、性能计时、周期任务
- `FileUtils` / `StringUtils` / `ByteUtils` / `NetUtils`：通用工具函数

## 上层接入建议

- L1 hardware 所有接口返回 `Result<T>` 或 `Result<void>`
- L2/L3 设备相关状态通过 `EventBus` 发布在线/离线事件
- L4 media 使用 `ThreadPool` 或 `Worker` 组织采集、解码、编码线程
- L7/L8 协议层把 `ErrorCode` 映射为协议响应码
