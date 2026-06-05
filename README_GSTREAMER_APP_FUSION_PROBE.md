# l8_gstreamer_app_fusion_probe

这个测试程序用于验证第一版真正融合链路：

```text
可见光 /dev/video3 MJPG
  -> GStreamer mppjpegdec
  -> RGB appsink

复合相机 /dev/video1 YUY2
  -> GStreamer videoconvert
  -> RGB appsink

C++
  -> 两路 RGB 取帧
  -> 复合画面 resize 到可见光尺寸
  -> alpha blend 简单融合
  -> fused RGB 转 NV12

GStreamer
  -> appsrc NV12
  -> mpph264enc
  -> h264parse
  -> mpegtsmux
  -> udpsink
```

## 编译

```bash
cd ~/PRJ/tri_fusion_camera_l1_integrated_mode_config_mgr
cmake --build build-arm64 --target l8_gstreamer_app_fusion_probe -j$(nproc)
```

如果 CMake 提示 `GStreamer app/video development packages not found`，说明交叉 sysroot 或板端开发环境缺少：

```text
gstreamer-1.0
gstreamer-app-1.0
gstreamer-video-1.0
```

需要在 Buildroot/rootfs 或交叉编译 sysroot 中加入 GStreamer 开发头文件和 pkg-config 文件。

## 运行

先确保没有旧程序占用摄像头：

```bash
killall gst-launch-1.0 2>/dev/null
killall l8_private_gstreamer_control_probe 2>/dev/null
fuser -v /dev/video1 /dev/video3
```

启动融合测试：

```bash
./l8_gstreamer_app_fusion_probe \
  --visible-device /dev/video3 \
  --composite-device /dev/video1 \
  --host 192.168.1.153 \
  --port 5004 \
  --visible-width 1920 \
  --visible-height 1080 \
  --composite-width 800 \
  --composite-height 600 \
  --fps 30 \
  --alpha 0.35
```

Windows 播放：

```powershell
.\ffplay.exe -fflags nobuffer -flags low_delay -framedrop -probesize 500000 -analyzeduration 1000000 "udp://0.0.0.0:5004?listen=1"
```

## 参数说明

- `--alpha 0.35`：复合相机画面占比 35%，可见光画面占比 65%。
- `--convert videoconvert`：默认使用 GStreamer 的 `videoconvert` 转 RGB。
- `--convert rgaconvert`：如果系统有 RGA GStreamer 插件，可以尝试替换。
- `--verbose`：每秒打印一次帧处理信息。

## 当前限制

1. 这是验证版，不是最终产品版。
2. 当前融合算法只是最近邻缩放 + RGB alpha blend。
3. 当前没有做精确 FrameSync，只是每轮各取一帧。
4. 当前没有做标定矩阵/单应性配准，只是简单拉伸对齐。
5. 当前 RGB 融合和 RGB->NV12 在 CPU 上做，后续产品版应改成 RGA/DMABUF/MPP 零拷贝链路。
