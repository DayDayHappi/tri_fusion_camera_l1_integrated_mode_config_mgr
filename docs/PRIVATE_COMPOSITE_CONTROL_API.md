# 私有协议：复合相机图像参数控制与配置读取接口

本版本在原有七种工作模式接口之外，增加 M4153 复合相机图像参数控制与当前配置读取能力。控制命令经 HTTP 私有协议进入板端，再由 `CompositeSensorController` 通过串口读写 ISP 寄存器。

## 串口寄存器依据

写寄存器使用命令 `0x5AF9`，读寄存器使用命令 `0x5AF8`。串口通信参数为 115200bps、8N1。

本版本涉及的常用寄存器：

| 序号 | 名称 | 地址 | 取值 |
|---:|---|---|---|
| 1 | 融合模式 | `0x6500` | `1` 红外，`2` 微光，`3` 融合 |
| 2 | 融合颜色 | `0x6508` | `1` 黑白，`2` 森林，`3` 雪地，`4` 海洋，`5` 城市，`6` 荒漠 |
| 3 | 轮廓模式 | `0x652C` | `0` 关，`1` 红，`2` 绿，`3` 蓝，`4` 紫 |
| 4 | 红外极性 | `0x6504` | `0` 白热，`1` 黑热 |
| 5 | 红外校正 | `0x650C` | `1` 校正，写后恢复 `0` |
| 6 | 红外亮度 | `0x651C` | `0~100` |
| 7 | 红外对比度 | `0x6520` | `0~100` |
| 8 | 微光亮度 | `0x6514` | `0~100` |
| 9 | 微光对比度 | `0x6518` | `0~100` |
| 10 | 红外配准挡位 | `0x6730` | `0~15` |
| 11 | 红外配准 x 偏移 | `0x6732` | 有符号 16bit |
| 12 | 红外配准 y 偏移 | `0x6734` | 有符号 16bit |
| 13 | 微光配准挡位 | `0x6736` | `0~10` |
| 14 | 微光配准 x 偏移 | `0x6738` | 有符号 16bit |
| 15 | 微光配准 y 偏移 | `0x673A` | 有符号 16bit |

## HTTP 接口

### 1. 融合颜色

```bash
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/black_white
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/forest
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/snow
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/ocean
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/city
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/fusion_color/desert
```

### 2. 轮廓模式

```bash
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/off
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/red
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/green
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/blue
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/contour/purple
```

兼容路径名称：`outline`、`contour_mode`。

### 3. 红外极性

```bash
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/infrared_polarity/white_hot
curl.exe -X POST http://192.168.1.21:18080/api/v1/composite/infrared_polarity/black_hot
```

兼容路径名称：`ir_polarity`、`polarity`。

### 4. 读取当前复合相机配置

```bash
curl.exe http://192.168.1.21:18080/api/v1/composite/config
```

该接口会按顺序读取常用寄存器 1~15，并返回给上位机。读取过程使用串口读寄存器命令 `0x5AF8`。

## 写参数成功返回示例

```json
{
  "ok": true,
  "result": "succeeded",
  "control": "fusion_color",
  "value_name": "forest",
  "value": 2,
  "message": "succeeded: composite parameter applied: fusion_color=forest"
}
```

## 写参数失败返回示例

```json
{
  "ok": false,
  "result": "failed",
  "control": "fusion_color",
  "value_name": "forest",
  "value": 2,
  "message": "serial write register failed: serial read timeout"
}
```

## 读取配置成功返回示例

```json
{
  "ok": true,
  "result": "succeeded",
  "message": "succeeded: composite config read successfully",
  "registers": [
    {
      "index": 1,
      "name": "fusion_mode",
      "display_name": "融合模式",
      "address": "0x6500",
      "raw_value": 3,
      "value": 3,
      "value_name": "fusion"
    },
    {
      "index": 2,
      "name": "fusion_color",
      "display_name": "融合颜色",
      "address": "0x6508",
      "raw_value": 2,
      "value": 2,
      "value_name": "forest"
    }
  ]
}
```

## 状态查询

```bash
curl.exe http://192.168.1.21:18080/api/v1/status
```

返回中增加：

```json
{
  "current_composite": {
    "fusion_color": "forest",
    "contour": "off",
    "infrared_polarity": "white_hot"
  },
  "composite_controls": {
    "fusion_color": "/api/v1/composite/fusion_color/{black_white|forest|snow|ocean|city|desert}",
    "contour": "/api/v1/composite/contour/{off|red|green|blue|purple}",
    "infrared_polarity": "/api/v1/composite/infrared_polarity/{white_hot|black_hot}",
    "query_config": "/api/v1/composite/config"
  }
}
```

## 注意事项

1. 写参数命令不重启 GStreamer pipeline，只通过串口写复合相机寄存器。
2. 读取配置命令不修改复合相机状态，只串口读取常用寄存器 1~15。
3. 如果启动程序使用了 `--no-serial`，这些图像参数命令和配置读取命令会返回失败。
4. 写参数接口现在明确返回 `result: "succeeded"` 或 `result: "failed"`，后端可以直接按该字段判断执行结果。
5. 读取偏移寄存器时，返回字段 `raw_value` 是寄存器原始 16bit 值，`value` 是按有符号 16bit 转换后的值。
