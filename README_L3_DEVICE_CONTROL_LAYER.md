# Tri-Fusion Camera L3 Device Control Layer v2

本版根据 M4153 UART ISP 寄存器协议重写 L3：不再把“串口写入成功”当作“模式切换成功”，而是严格执行：发送寄存器读/写帧 -> 接收应答帧 -> 校验包头、长度、原命令、执行状态、16bit 累加和。

## 关键协议

- 读寄存器发送帧：`55 AA 00 06 5A F8 addr_H addr_L checksum_H checksum_L`
- 写寄存器发送帧：`55 AA 00 08 5A F9 addr_H addr_L data_H data_L checksum_H checksum_L`
- 写寄存器成功 ACK：`55 AA 00 06 5A F9 58 58 B3 57`
- 写寄存器失败 ACK：`55 AA 00 06 5A F9 4B 4B A6 50`
- 读寄存器 ACK：`55 AA 00 08 5A F8 status_H status_L data_H data_L checksum_H checksum_L`

校验和按 16bit 大端字累加，只累加“数据长度 + 命令 + 地址/状态/数据”等字段，不包含包头。

## 已新增接口

`CompositeSensorCommandBuilder`：

- `buildSetOutputMode(mode)`：内部生成写 `0x6500` 的标准 M4153 写寄存器帧。
- `buildQueryOutputMode()`：内部生成读 `0x6500` 的标准读寄存器帧。
- `buildReadRegister(address)` / `buildWriteRegister(address, value)`。

`CompositeSensorCommandParser`：

- `parseWriteRegisterAck(bytes)`：严格校验 10 字节写 ACK。
- `parseReadRegisterAck(bytes)`：严格校验 12 字节读 ACK。
- `parseModeReport(bytes)`：从读 `0x6500` 的 ACK 中解析 1=红外、2=微光、3=融合。

`CompositeSensorController`：

- `setOutputMode()` / `queryOutputMode()`。
- `readRegister()` / `writeRegister()`。
- `setFusionColor()`、`setInfraredPolarity()`、`triggerInfraredCorrection()`。
- `setInfraredBrightness()`、`setInfraredContrast()`、`setLowlightBrightness()`、`setLowlightContrast()`。
- `setInfraredRegistrationZoom()`、`setInfraredRegistrationOffsetX/Y()`。
- `setLowlightRegistrationZoom()`、`setLowlightRegistrationOffsetX/Y()`。

注意：你提供的协议摘录没有给出“保存配置”寄存器地址，因此 `saveCurrentConfig()` / `restoreDefaultConfig()` 目前返回 `Unsupported`，等拿到真实地址后把 `CompositeSensorRegister::SaveConfig` 从 `0xFFFF` 改成厂家地址即可。

## 上板测试

```bash
cmake -S . -B build
cmake --build build -j
./build/l3_device_control_smoke_test

sudo ./build/l3_composite_control_probe ./configs mode thermal
sudo ./build/l3_composite_control_probe ./configs mode lowlight
sudo ./build/l3_composite_control_probe ./configs mode composite
sudo ./build/l3_composite_control_probe ./configs read 0x6500
sudo ./build/l3_composite_control_probe ./configs write 0x651C 50
```

本版的 `l3_composite_control_probe` 调用的是 `CompositeSensorController`，不是直接调用 L1 `SerialPort`。ACK 校验逻辑在 L3 Parser/Controller 内部完成。
