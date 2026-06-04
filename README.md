# 三光融合相机板端联调工程 README

当前工程用于 RK3588/RK3568 板端联调三光融合相机的视频输出、复合相机串口控制、上位机 HTTP 控制和运行日志追踪。

当前主要运行程序：

```bash
l8_private_gstreamer_control_probe
```

该程序是当前联调主入口，集成：

```text
上位机 HTTP 控制
    ↓
板端模式切换
    ↓
复合相机串口控制
    ↓
GStreamer 视频采集 / 编码 / UDP 推流
    ↓
日志文件记录
```

---

## 1. 当前已实现功能

### 1.1 上位机 HTTP 控制服务

默认监听端口：

```text
18080
```

状态查询接口：

```bash
curl.exe http://192.168.1.21:18080/api/v1/status
```

支持 7 种工作模式切换：

| 编号 | 模式 | HTTP 接口 |
|---:|---|---|
| 1 | 可见光 | `/api/v1/mode/visible` |
| 2 | 微光 | `/api/v1/mode/lowlight` |
| 3 | 热红外 | `/api/v1/mode/thermal` |
| 4 | 微光 + 热红外复合 | `/api/v1/mode/lowlight_thermal` |
| 5 | 可见光 + 微光 | `/api/v1/mode/visible_lowlight` |
| 6 | 可见光 + 热红外 | `/api/v1/mode/visible_thermal` |
| 7 | 可见光 + 复合 | `/api/v1/mode/visible_composite` |

> 当前联调版本主要是单路视频切换输出。可见光 + 微光 / 可见光 + 热红外 / 可见光 + 复合的双路算法融合链路尚未真正启用。

---

### 1.2 GStreamer 视频输出

视频通过 UDP 推到上位机，默认参数：

```text
host = 192.168.1.153
port = 5004
```

当前可见光路径：

```text
/dev/video3
→ image/jpeg
→ mppjpegdec
→ mpph264enc
→ h264parse
→ mpegtsmux
→ udpsink 192.168.1.153:5004
```

当前微光 / 红外 / 微光红外复合路径：

```text
复合相机 video node，默认由 configs/camera.yaml 决定
→ video/x-raw,format=YUY2
→ mpph264enc
→ h264parse
→ mpegtsmux
→ udpsink 192.168.1.153:5004
```

注意：当前工程没有显式插入 `videoconvert` 或 `rgaconvert`，所以代码层面没有明确的 `YUY2 -> NV12` 节点。实际格式协商需要结合 GStreamer verbose 日志确认。

---

### 1.3 复合相机串口模式控制

支持通过串口控制 M4153 复合相机输出模式：

```text
微光输出
热红外输出
微光 + 热红外复合输出
```

默认串口参数来自：

```text
configs/serial.yaml
```

运行时也可以通过参数覆盖：

```bash
--serial-dev /dev/ttyACM0
--serial-baud 115200
--serial-timeout-ms 1000
--serial-retry 1
```

程序会等待串口 ACK，并根据返回状态判断成功 / 失败。

---

### 1.4 复合相机参数控制

支持以下上位机控制接口：

#### 融合颜色

```bash
/api/v1/composite/fusion_color/{black_white|forest|snow|ocean|city|desert|default}
```

取值：

| 参数 | 含义 | 值 |
|---|---|---:|
| `black_white` | 黑白 | 1 |
| `forest` | 森林 | 2 |
| `snow` | 雪地 | 3 |
| `ocean` | 海洋 | 4 |
| `city` | 城市 | 5 |
| `desert` | 荒漠 | 6 |
| `default` | 默认 | 7 |

#### 轮廓模式

```bash
/api/v1/composite/contour/{off|red|green|blue|purple}
```

#### 红外极性

```bash
/api/v1/composite/infrared_polarity/{white_hot|black_hot}
```

---

### 1.5 读取当前复合相机配置

读取接口：

```bash
curl.exe http://192.168.1.21:18080/api/v1/composite/config
```

当前读取 15 项常用配置：

| 序号 | 配置项 | 寄存器 |
|---:|---|---|
| 1 | 融合模式 | `0x6500` |
| 2 | 融合颜色 | `0x6508` |
| 3 | 轮廓模式 | `0x652C` |
| 4 | 红外极性 | `0x6504` |
| 5 | 红外校正 | `0x650C` |
| 6 | 红外亮度 | `0x651C` |
| 7 | 红外对比度 | `0x6520` |
| 8 | 微光亮度 | `0x6514` |
| 9 | 微光对比度 | `0x6518` |
| 10 | 红外配准挡位 | `0x6730` |
| 11 | 红外 X 偏移 | `0x6732` |
| 12 | 红外 Y 偏移 | `0x6734` |
| 13 | 微光配准挡位 | `0x6736` |
| 14 | 微光 X 偏移 | `0x6738` |
| 15 | 微光 Y 偏移 | `0x673A` |

---

### 1.6 运行日志写入文件

默认不再直接把日志刷到终端，而是写入当前运行目录下的：

```text
./log/l8_private_gstreamer_control_probe.log
```

日志内容包括：

```text
[VIDEO][CONFIG]   配置加载与模式映射
[VIDEO][START]    pipeline 启动参数
[VIDEO][PATH]     视频数据路径阶段
[VIDEO][GST-CMD]  实际 gst-launch 命令
[VIDEO][PROCESS]  gst-launch 进程启动 / 停止
[SERIAL]          串口读写命令、ACK 原始帧、解析结果
[CONTROL]         上位机 HTTP 控制请求
```

可选参数：

```bash
--log-dir /root/log
--log-file video_trace.log
--console-log
```

`--console-log` 表示临时恢复终端输出。

---

## 2. 编译命令

在开发 Ubuntu 上执行：

```bash
cd ~/tri_fusion_camera_l1_integrated_mode_config_mgr

rm -rf build-arm64

cmake -S . -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=toolchain-aarch64.cmake \
  -DTRI_FUSION_BUILD_TESTS=ON \
  -DTRI_FUSION_BUILD_TOOLS=ON

cmake --build build-arm64 -j$(nproc)
```

生成的目标程序：

```text
build-arm64/l8_private_gstreamer_control_probe
```

---

## 3. 拷贝到板子

板子 IP 当前按 `192.168.1.21` 编写：

```bash
scp build-arm64/l8_private_gstreamer_control_probe root@192.168.1.21:/root/
scp -r configs root@192.168.1.21:/root/
```

---

## 4. 板端运行命令

登录板子：

```bash
ssh root@192.168.1.21
cd /root
chmod +x ./l8_private_gstreamer_control_probe
```

正常启动：

```bash
./l8_private_gstreamer_control_probe \
  --config-dir /root/configs \
  --host 192.168.1.153 \
  --port 5004 \
  --control-port 18080 \
  --serial-dev /dev/ttyACM0 \
  --serial-baud 115200 \
  --stable-ms 500
```

指定日志目录和文件名：

```bash
./l8_private_gstreamer_control_probe \
  --config-dir /root/configs \
  --host 192.168.1.153 \
  --port 5004 \
  --control-port 18080 \
  --serial-dev /dev/ttyACM0 \
  --serial-baud 115200 \
  --stable-ms 500 \
  --log-dir /root/log \
  --log-file video_trace.log
```

临时恢复终端日志：

```bash
./l8_private_gstreamer_control_probe \
  --config-dir /root/configs \
  --host 192.168.1.153 \
  --port 5004 \
  --control-port 18080 \
  --serial-dev /dev/ttyACM0 \
  --serial-baud 115200 \
  --stable-ms 500 \
  --console-log
```

不建议常规使用 `--no-ack`。调试串口返回成功 / 失败时，应保持 ACK 检查开启。

---

## 5. 查看日志

默认日志：

```bash
tail -f /root/log/l8_private_gstreamer_control_probe.log
```

如果在 `/root` 下启动且未指定 `--log-dir`，也可以查看：

```bash
tail -f ./log/l8_private_gstreamer_control_probe.log
```

只看视频路径：

```bash
grep "\[VIDEO\]" ./log/l8_private_gstreamer_control_probe.log
```

只看串口 ACK：

```bash
grep "\[SERIAL\]" ./log/l8_private_gstreamer_control_probe.log
```

只看上位机控制请求：

```bash
grep "\[CONTROL\]" ./log/l8_private_gstreamer_control_probe.log
```

---

## 6. 上位机播放命令

Windows 上位机进入 ffmpeg 目录：

```powershell
cd E:\ffmpeg\ffmpeg-7.1.1-full_build\bin
```

启动播放：

```powershell
.\ffplay.exe -fflags nobuffer -flags low_delay -framedrop -probesize 500000 -analyzeduration 1000000 "udp://0.0.0.0:5004?listen=1"
```

---

## 7. 上位机控制命令

### 7.1 查询状态

```powershell
curl.exe http://192.168.1.21:18080/api/v1/status
```

### 7.2 模式切换

```powershell
curl.exe -X POST http://192.168.1.21:18080/api/v1/mode/visible
curl.exe -X POST http://192.168.1.21:18080/api/v1/mode/lowlight
curl.exe -X POST http://192.168.1.21:18080/api/v1/mode/thermal
curl.exe -X POST http://192.168.1.21:18080/api/v1/mode/lowlight_thermal
curl.exe -X POST http://192.168.1.21:18080/api/v1/mode/visible_lowlight
curl.exe -X POST http://192.168.1.21:18080/api/v1/mode/visible_thermal
curl.exe -X POST http://192.168.1.21:18080/api/v1/mode/visible_composite
```

### 7.3 融合颜色控制

```powershell
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/black_white
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/forest
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/snow
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/ocean
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/city
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/desert
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/default
```

### 7.4 轮廓模式控制

```powershell
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/off
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/red
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/green
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/blue
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/purple
```

### 7.5 红外极性控制

```powershell
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/infrared_polarity/white_hot
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/infrared_polarity/black_hot
```

### 7.6 读取完整配置

```powershell
curl.exe http://192.168.1.21:18080/api/v1/composite/config
```

---

## 8. 典型返回格式

成功：

```json
{
  "ok": true,
  "result": "succeeded",
  "message": "succeeded: composite parameter applied"
}
```

失败：

```json
{
  "ok": false,
  "result": "failed",
  "message": "serial write register failed: serial read timeout"
}
```

---

## 9. 关键配置文件

```text
configs/camera.yaml      摄像头节点、格式、分辨率、帧率
configs/serial.yaml      串口设备、波特率、超时、重试
configs/media.yaml       媒体参数
configs/mode.yaml        模式相关配置
configs/protocol.yaml    协议相关配置
```

当前建议：

```text
可见光节点：/dev/video3
复合相机节点：以 configs/camera.yaml 为准，当前常用 /dev/video1
串口节点：/dev/ttyACM0
控制端口：18080
视频 UDP 端口：5004
```

---

## 10. 当前版本重点说明

1. 当前程序用于板端联调，不是最终产品主程序。
2. 当前可见光使用 MJPEG 输入，走 `mppjpegdec -> mpph264enc`。
3. 当前复合相机使用 YUY2 输入，直接送 `mpph264enc`。
4. 当前没有显式 `YUY2 -> NV12` 转换节点。
5. 当前模式切换会停止旧 GStreamer pipeline，再启动新 pipeline。
6. 复合相机参数控制不会重启视频 pipeline，只写串口寄存器。
7. 日志默认写入文件，便于长期跟踪视频路径和串口 ACK。
8. `--no-ack` 只适合临时绕过串口 ACK 调试，不建议正常联调使用。
