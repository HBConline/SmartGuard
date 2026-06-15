# SmartGuard · 智能门卫监控系统

> 边缘 AI 物联网方案：OpenCV DNN 人脸检测 + RTMP 视频推流 + OneNET MQTT 云端上报

[![Platform](https://img.shields.io/badge/platform-Orange%20Pi%204%20Pro-orange)](https://github.com/HBConline/SmartGuard)
[![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20Python%20%7C%20HTML-blue)](https://github.com/HBConline/SmartGuard)

---

## 功能特性

| 功能 | 状态 |
|------|------|
| 🧠 人脸检测 (SSD Caffe) + 人体检测 (MobileNet SSD) | ✅ |
| 👥 人数统计 + 停留计时 | ✅ |
| 🚨 超时告警（红色闪烁） | ✅ |
| 📊 人数趋势图 | ✅ |
| 📝 事件日志 | ✅ |
| 📡 RTMP 视频推流到 SRS | ✅ |
| ☁️ OneNET MQTT 数据上报 | ✅ |
| 📱 微信小程序 / Web 仪表板 | ✅ |
| 🎨 GitHub Dark 主题 UI | ✅ |

## 系统架构

```
摄像头 OV13850 → V4L2 驱动 → 640x480 BGR
    ├── AI 推理 (5 FPS) → 人数/计时 → Qt5 UI
    ├── ffmpeg → H.264 → RTMP → SRS → 小程序直播
    └── /tmp/seat_state.json → MQTT → OneNET → Web 仪表板
```

## 硬件需求

| 组件 | 规格 |
|------|------|
| 主控 | Orange Pi 4 Pro (Allwinner H616, 8核, 3.7GB RAM) |
| 摄像头 | OV13850 MIPI CSI, 2112x1568@30fps |
| 网络 | WiFi / 以太网 |

## 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/HBConline/SmartGuard.git
cd SmartGuard
```

### 2. 配置环境变量

```bash
cp .env.example .env
# 编辑 .env 填入你的 OneNET 和 SRS 配置
```

### 3. 安装依赖

```bash
# 系统依赖 (Orange Pi / Debian)
sudo apt install qtbase5-dev libopencv-dev g++ make ffmpeg

# Python 依赖
pip3 install paho-mqtt
```

### 4. 编译

```bash
make -f Makefile_smart
```

### 5. 启动

```bash
# 加载摄像头驱动
sudo modprobe vin_v4l2

# 启动 OneNET 数据上报（开机自启可选）
sudo systemctl start onenet

# 启动主程序
DISPLAY=:0 ./smartguard
```

## 文件结构

```
SmartGuard/
├── smartguard.cpp              # 主程序 (Qt5 + OpenCV + 推流)
├── v4l2_camera.cpp/h           # V4L2 摄像头驱动
├── Makefile_smart              # 编译脚本
├── mqtt_client.py              # OneNET MQTT 数据上报
├── onetoken.py                 # OneNET HMAC-SHA1 令牌生成
├── index.html                  # Web 监控仪表板
├── seats.json                  # 座位 ROI 配置
├── face_deploy.prototxt        # 人脸检测模型结构
├── face_detector.caffemodel    # 人脸检测模型权重 (~5MB)
├── MobileNetSSD_deploy.*       # 人体检测模型 (~22MB)
├── onenet.service              # systemd 服务文件
├── SmartGuard_Report.md        # 详细技术报告
├── .env.example                # 环境变量模板
└── README.md
```

## 技术栈

- **AI 推理**: OpenCV DNN + Caffe 模型
- **UI 框架**: Qt5 (GitHub Dark 主题)
- **视频推流**: ffmpeg (H.264 + RTMP + FLV)
- **物联网**: 中国移动 OneNET (MQTT + HTTP API)
- **前端**: 原生 HTML/CSS/JS (flv.js + HLS)
- **构建**: GNU Make + g++

## 配置说明

部署前需要替换以下配置中的占位符：

1. **`.env`** — OneNET 产品 ID、设备名称、设备密钥
2. **`smartguard.cpp`** — RTMP 推流地址
3. **`index.html`** — API 密钥、SRS 服务器 IP、FLV 拉流地址
4. **`onenet.service`** — Python 脚本路径

详见 `.env.example` 和 [SmartGuard.md](SmartGuard.md)。

## 许可证

GPL-2.0-or-later (V4L2 驱动部分)

---

*Made with ❤️ on Orange Pi 4 Pro*
