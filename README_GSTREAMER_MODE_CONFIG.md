# GStreamer 模式源配置版说明

本版新增 `GStreamerPipelineConfigManager`，模式切换时不再固定使用启动参数里的单一摄像头节点。
程序会读取 `configs/camera.yaml`：

- `camera.visible.video_node`：可见光相机节点，当前默认 `/dev/video3`，MJPG 1920x1080@30。
- `camera.composite_low_thermal.video_node`：微光/热红外复合相机图像节点，当前默认 `/dev/video1`，YUY2 800x600@30。
- `camera.mode_source.*`：上位机模式命令到视频采集源的映射。

控制命令仍然是：

```bash
curl.exe -X POST http://<板子IP>:18080/api/v1/mode/visible
curl.exe -X POST http://<板子IP>:18080/api/v1/mode/lowlight
curl.exe -X POST http://<板子IP>:18080/api/v1/mode/thermal
curl.exe -X POST http://<板子IP>:18080/api/v1/mode/lowlight_thermal
curl.exe -X POST http://<板子IP>:18080/api/v1/mode/visible_lowlight
curl.exe -X POST http://<板子IP>:18080/api/v1/mode/visible_thermal
curl.exe -X POST http://<板子IP>:18080/api/v1/mode/visible_composite
```

板端运行示例：

```bash
/root/l8_private_gstreamer_control_probe \
  --config-dir /root/configs \
  --host 192.168.1.153 \
  --port 5004 \
  --control-port 18080 \
  --serial-dev /dev/ttyACM0 \
  --serial-baud 115200 \
  --stable-ms 500
```

可见光模式会使用 `/dev/video3` 的 MJPG pipeline：

```text
v4l2src device=/dev/video3 ! image/jpeg,width=1920,height=1080,framerate=30/1 ! jpegdec ! videoconvert ! mpph264enc ! h264parse ! mpegtsmux ! udpsink host=192.168.1.153 port=5004
```

微光/热红外/复合模式会使用 `/dev/video1` 的 YUY2 pipeline，并在启动视频前通过串口切换复合相机输出模式。
