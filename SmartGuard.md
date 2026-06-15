# SmartGuard 智能门卫 — 完整项目文档

> Orange Pi 4 Pro | Qt5 C++ | OpenCV DNN | OneNET MQTT | SRS 推流 | 微信小程序

---

## 一、硬件

| 组件 | 规格 |
|------|------|
| 主控 | Orange Pi 4 Pro (Allwinner H616, aarch64, 8核, 3.7GB RAM) |
| 摄像头 | OV13850 MIPI CSI, 2112x1568@30fps, `/dev/video8` |
| 网络 | WiFi 连接 OneNET + SRS 推流 |

---

## 二、功能

| 功能 | 状态 |
|------|------|
| 人脸检测 (SSD Caffe) | 已完成 |
| 人数统计 + 停留计时 | 已完成 |
| 超30秒红色闪烁告警 | 已完成 |
| 人数趋势图 | 已完成 |
| 事件日志 | 已完成 |
| RTMP 视频推流到 SRS | 已整合 |
| OneNET 数据上报 (MQTT) | 已完成 |
| 微信小程序数据 + 视频 | 已对接 |
| GitHub Dark 主题 UI | 已完成 |
| 登录保护 | admin/123456 |

---

## 三、文件结构

```
~/Desktop/
├── smartguard.cpp            # 主程序 (Qt5 + OpenCV + 推流)
├── Makefile_smart            # 编译脚本
├── smartguard                # 可执行文件
├── v4l2_camera.cpp/h         # V4L2 摄像头驱动
├── face_deploy.prototxt      # 人脸检测模型
├── face_detector.caffemodel  # 人脸检测权重 (5.2MB)
├── MobileNetSSD_deploy.*     # 人体检测 (备用)
├── SmartGuard.md             # 本文档
│
~/onenet/
├── onetoken.py               # OneNET Token 生成
├── mqtt_client.py            # MQTT 数据上报
├── /etc/systemd/system/onenet.service
```

---

## 四、启动命令

```bash
# 加载摄像头驱动
modprobe vin_v4l2

# 启动 OneNET 上报（开机自启）
systemctl start onenet

# 编译并启动 SmartGuard
make -f Makefile_smart
DISPLAY=:0 ./smartguard
```

---

## 五、OneNET 配置

| 参数 | 值 |
|------|-----|
| Product ID | `<YOUR_PRODUCT_ID>` |
| Device | `<YOUR_DEVICE_NAME>` |
| Access Key | `<YOUR_ACCESS_KEY>` |
| MQTT Broker | `<YOUR_PRODUCT_ID>.mqttstls.acc.cmcconenet.cn:8883` |
| 上报 Topic | `$sys/<YOUR_PRODUCT_ID>/<YOUR_DEVICE_NAME>/thing/property/post` |

| 属性 | 类型 | 说明 |
|------|------|------|
| `online_status` | enum | 0离线 1在线 |
| `person_count` | int32 | 当前人数 |
| `seat_status` | enum | 0空闲 1占用 |
| `timing` | int32 | 停留计时(秒) |

---

## 六、视频推流

| 参数 | 值 |
|------|-----|
| SRS 服务器 | `<YOUR_SRS_SERVER_IP>` |
| RTMP | `rtmp://<YOUR_SRS_SERVER_IP>/live/camera` |
| 小程序拉流 | `http://<YOUR_SRS_SERVER_IP>:8080/live/camera.flv` |
| 分辨率 | 320x240 |
| 编码 | H.264 ultrafast |
| 帧率 | ~15 FPS |
| 码率 | 600kbps |

---

## 七、小程序配置

```html
<!-- 视频 -->
<live-player src="http://<YOUR_SRS_SERVER_IP>:8080/live/camera.flv"
  mode="live" autoplay muted />

<!-- 数据 API -->
GET https://open.iot.10086.cn/thingmodel/device/property/query
  ?product_id=<YOUR_PRODUCT_ID>
  &device_name=<YOUR_DEVICE_NAME>
Authorization: <YOUR_ACCESS_KEY>
```

---

## 八、数据流架构

```
摄像头 → V4L2 → 320x240 BGR
    ├── ffmpeg stdin → H.264 → RTMP → SRS → 小程序 live-player
    ├── OpenCV DNN (5 FPS) → 人数/计时 → Qt UI
    └── /tmp/seat_state.json → paho-mqtt → OneNET → 小程序 API
```
