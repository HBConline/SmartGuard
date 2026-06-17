# 🛡️ SmartGuard

<div align="center">

> **Autonomous Edge Inference Gateway** — 异构计算架构下的实时人员感知与流媒体分发系统

[![Architecture](https://img.shields.io/badge/arch-ARMv8--A%20%7C%20Allwinner%20H616%20Octa--Core-brightgreen)](https://github.com/HBConline/SmartGuard)
[![Runtime](https://img.shields.io/badge/runtime-Qt5%20%2B%20OpenCV%20DNN%20%2B%20ffmpeg-blueviolet)](https://github.com/HBConline/SmartGuard)
[![Protocol](https://img.shields.io/badge/protocol-RTMP%20%7C%20MQTT%203.1.1%20%7C%20HTTP--FLV-informational)](https://github.com/HBConline/SmartGuard)
[![Inference](https://img.shields.io/badge/inference-SSD%20Caffe%20%7C%20MobileNet%20Backbone-red)](https://github.com/HBConline/SmartGuard)
[![License](https://img.shields.io/badge/license-GPL--2.0--or--later-lightgrey)](https://github.com/HBConline/SmartGuard)

</div>

---

## 📡 系统概要

**SmartGuard** 是一款部署于 ARM 边缘计算节点的**自主环境感知引擎**，以 **Allwinner H616 8 核处理器** 为算力基座，在 3.7 GB 内存约束下实现 **深度神经网络推理**、**低延迟视频编码管线** 与 **物联网遥测同步** 的三位一体协同计算。

系统采用 **C++ Qt5 主进程 + Python MQTT 守护进程** 的异构多进程架构，通过内核级 **V4L2 Multi-Plane DMA** 零拷贝采集原始 YUV420M 视频帧，在 **OpenCV DNN 后端** 上执行 SSD Face Detector / MobileNet SSD 双模型级联推理，并向 **SRS 流媒体边缘节点** 实时推送 H.264 硬编码视频流。

---

## 🧬 技术架构

```
┌─────────────────────────────────────────────────────────────┐
│                     SmartGuard 计算拓扑                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────┐    ┌─────────────┐    ┌────────────────────┐  │
│  │ OV13850  │───▶│  V4L2 MPLANE │───▶│  YUV420M → BGR24   │  │
│  │ MIPI CSI │    │  DMA ZeroCopy│    │  Colorspace Conv   │  │
│  │ 2112×1568│    │  640×480@30f │    │  ISP Offload       │  │
│  └──────────┘    └─────────────┘    └───────┬────────────┘  │
│                                              │              │
│                    ┌─────────────────────────┼───────┐      │
│                    │                    ┌────▼─────┐ │      │
│                    │  ┌────────────────┤ Inference │ │      │
│                    │  │  1:3 Decimator │ Pipeline  │ │      │
│                    │  │  AI @ 5 FPS    │  ● SSD Face│ │      │
│                    │  │                │  ● MobileNet│ │      │
│                    │  └────────────────└────┬─────┘ │      │
│                    │                       │        │      │
│                    │  ┌────────────────────▼──────┐ │      │
│                    │  │     Qt5 HMI Renderer       │ │      │
│                    │  │  ● Real-time Video Overlay │ │      │
│                    │  │  ● Occupancy Analytics     │ │      │
│                    │  │  ● Threshold Alarm Engine  │ │      │
│                    │  │  ● Temporal Trend Graph    │ │      │
│                    │  └────────────────┬──────────┘ │      │
│                    │                   │            │      │
│  ┌─────────────────▼───┐    ┌─────────▼──────────┐ │      │
│  │   ffmpeg Subprocess  │    │  Telemetry Daemon   │ │      │
│  │   stdin Pipe @15FPS  │    │  mqtt_client.py     │ │      │
│  │   H.264 ultrafast    │    │  HMAC-SHA1 Auth     │ │      │
│  │   zerolatency tuned  │    │  OneJSON Protocol   │ │      │
│  │   RTMP → SRS CDN     │    │  OneNET MQTT Broker │ │      │
│  └─────────┬────────────┘    └──────────┬──────────┘ │      │
│            │                            │            │      │
└────────────┼────────────────────────────┼────────────┘      │
             │                            │                   │
     ┌───────▼───────┐          ┌─────────▼──────────┐       │
     │  SRS Streaming │          │  OneNET IoT Cloud   │       │
     │  HTTP-FLV/HLS  │          │  Property Telemetry │       │
     └───────┬───────┘          └─────────┬──────────┘       │
             │                            │                   │
     ┌───────▼────────────────────────────▼──────────┐       │
     │          WeChat Mini Program / Web Dashboard    │       │
     │          flv.js Player + HTTP API Polling       │       │
     └────────────────────────────────────────────────┘       │
```

---

## ⚙️ 核心能力矩阵

| 子系统 | 技术指标 | 工程实现 |
|--------|----------|----------|
| 🔬 **推理引擎** | SSD Face Detector (Caffe) + MobileNet SSD Fallback, 置信度阈值 0.5, 输入张量 300×300×3 | `cv::dnn::blobFromImage` 预处理, 指针级 `float*` 输出解析 |
| 📹 **视频采集** | V4L2 MPLANE DMA 零拷贝, YUV420M 三平面格式, 4 缓冲区轮转 | 内核级 `VIDIOC_DQBUF/QBUF` ioctl, `mmap` 内存映射 |
| 🎞️ **编码管线** | H.264 Baseline Profile, 600 kbps CBR, GOP=30, ultrafast preset | QProcess stdin 管道, 零序列化原始帧注入 |
| 📡 **遥测上报** | OneJSON v1.0 物模型, 4 维属性向量, 10s 采集周期 | HMAC-SHA1 动态签名, paho-mqtt 异步事件循环 |
| 🖥️ **人机界面** | Qt5 Fusion 渲染引擎, GitHub Dark 设计语言, 66ms 主循环周期 | QTimer 驱动, QPixmap 帧缓冲交换, CSS-in-Qt 样式注入 |
| 🔐 **认证层** | 双因子设备认证 + 用户凭证校验 | OneNET Token 动态生成 (TTL 3600s), Qt Dialog 登录沙箱 |

---

## 📊 性能特征

| 度量 | 数值 | 备注 |
|------|------|------|
| 主循环频率 | ~15 FPS (66ms) | QTimer 驱动的帧采集周期 |
| AI 推理频率 | ~5 FPS | 3:1 抽样降频, ARM Cortex-A55 约束 |
| 推理延迟 | ~200ms/帧 | CPU-bound DNN forward pass |
| 推流分辨率 | 320×240 | cv::resize 下采样后管道注入 |
| 编码器延迟 | <50ms | ultrafast + zerolatency 调优 |
| 视频码率 | 600 kbps CBR | 低带宽场景优化 |
| 遥测上报间隔 | 10s | MQTT QoS 1, OneNET 平台同步 |
| 内存驻留 | <200 MB | 含双 Caffe 模型权重 (~27MB) |

---

## 🚀 快速部署

### 依赖矩阵

```bash
# 系统运行时
sudo apt install qtbase5-dev libopencv-dev g++ make ffmpeg

# Python 遥测栈
pip3 install paho-mqtt

# 内核模块
sudo modprobe vin_v4l2
```

### 环境配置

```bash
cp .env.example .env
# 注入 OneNET 产品凭证 + SRS 边缘节点地址
```

### 编译构建

```bash
make -f Makefile_smart
# MOC 元对象预编译 → g++ 交叉链接 Qt5Widgets + OpenCV4
```

### 服务编排

```bash
# 遥测守护进程 (systemd 托管, 自动重启)
sudo systemctl enable --now onenet

# 主控进程 (X11 会话绑定)
DISPLAY=:0 ./smartguard
```

---

## 🏗️ 仓库拓扑

```
SmartGuard/
├── smartguard.cpp                 # 主控二进制 — Qt5 UI 线程 + 推理管线 + 编码管道
├── v4l2_camera.{cpp,h}            # V4L2 抽象层 — MPLANE DMA 零拷贝驱动
├── Makefile_smart                 # 交叉编译规则 — MOC 预处理 → ELF 链接
├── mqtt_client.py                 # 遥测守护进程 — 异步 MQTT 属性上报
├── onetoken.py                    # 签名引擎 — HMAC-SHA1 令牌生成器
├── index.html                     # 监控仪表板 — flv.js 解码器 + OneNET REST 客户端
├── face_detector.caffemodel       # SSD Face Detector 权重 (~5.2 MB)
├── face_deploy.prototxt           # 人脸检测 Caffe 计算图定义
├── MobileNetSSD_deploy.caffemodel # MobileNet SSD Backbone 权重 (~22 MB)
├── MobileNetSSD_deploy.prototxt   # 目标检测 Caffe 计算图定义
├── seats.json                     # ROI 区域配置 (6 座位节点)
├── onenet.service                 # systemd 单元 — 网络就绪后自启
├── .env.example                   # 环境变量模板 — 凭证注入点
├── SmartGuard_Report.md           # 工程深度分析报告 (570 行)
└── README.md                      # 本文档
```

---

## 🛰️ 协议栈

```
┌──────────────────────────────────────┐
│           应用表示层                   │
│  OneJSON v1.0  │  FLV over HTTP       │
├──────────────────────────────────────┤
│           传输会话层                   │
│  MQTT 3.1.1   │  RTMP  │  HLS        │
├──────────────────────────────────────┤
│           编码封装层                   │
│  H.264 Baseline  │  FLV Container     │
├──────────────────────────────────────┤
│           采集抽象层                   │
│  V4L2 MPLANE  │  DMA Buffer Pool      │
└──────────────────────────────────────┘
```

---

## 📄 许可证

```
V4L2 驱动层     — GPL-2.0-or-later
应用层          — 保留所有权利
模型权重        — 遵循原始 Caffe Model Zoo 许可
```

---

<div align="center">

```
⚡ Edge Computing  ◆  Neural Inference  ◆  Real-time Streaming  ◆  IoT Telemetry ⚡
```

</div>
