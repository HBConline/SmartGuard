# SmartGuard 智能门卫 — 项目源码分析报告

> 版本：v2.0 | 语言：C++ Qt5 + Python | 平台：Orange Pi 4 Pro | 日期：2026-06-14

---

## 目录

1. [项目概述](#1-项目概述)
2. [smartguard.cpp 逐行分析](#2-smartguardcpp-逐行分析)
3. [v4l2_camera.cpp/h 分析](#3-v4l2_cameracpph-分析)
4. [mqtt_client.py 分析](#4-mqtt_clientpy-分析)
5. [onetoken.py 分析](#5-onetokenpy-分析)
6. [Makefile_smart 分析](#6-makefile_smart-分析)
7. [系统数据流](#7-系统数据流)
8. [OneNET 对接详解](#8-onenet-对接详解)
9. [SRS 视频推流详解](#9-srs-视频推流详解)
10. [性能优化分析](#10-性能优化分析)

---

## 1. 项目概述

SmartGuard 是一个运行在 Orange Pi 4 Pro 上的门口监控系统，集成：

- **AI 推理**：OpenCV DNN，优先使用 SSD Face Detector（人脸检测），备选 MobileNet SSD（人体检测）
- **Qt5 前端**：GitHub Dark 风格 UI，实时视频显示 + 人数统计 + 停留计时 + 告警
- **视频推流**：通过 ffmpeg 管道将实时画面推送到 SRS 服务器（RTMP/FLV），供小程序直播
- **OneNET 物联网**：通过 paho-mqtt 上报设备状态到中国移动 OneNET 平台，供小程序数据面板查询

---

## 2. smartguard.cpp 逐行分析

### 2.1 头文件（第 1-31 行）

```cpp
#include <QApplication>    // Qt 应用程序主循环
#include <QMainWindow>     // 主窗口基类
#include <QDialog>         // 登录/选择对话框
#include <QLabel>          // 文本/图片显示
#include <QPushButton>     // 按钮
#include <QTimer>          // 定时器（核心循环驱动）
#include <QImage/QPixmap>  // OpenCV Mat ↔ Qt 图像转换
#include <QProcess>         // 启动和管理 ffmpeg 子进程
#include <opencv2/dnn.hpp> // OpenCV 深度学习推理
#include "v4l2_camera.h"   // 自定义 V4L2 摄像头驱动
```

**关键分析**：`QProcess` 是整合推流的核心——不需要单独的 Python 推流脚本，一个 C++ 进程同时处理 UI + AI + 推流。

### 2.2 全局状态变量（第 35-37 行）

```cpp
int person_count=0, last_count=-1, peak_today=0;
double presence_start=0;
std::deque<int> history;
```

| 变量 | 类型 | 说明 |
|------|------|------|
| `person_count` | int | 当前检测到的人数 |
| `last_count` | int | 上一次人数（变化检测） |
| `peak_today` | int | 今日峰值人数 |
| `presence_start` | double | 有人状态开始的时间戳 |
| `history` | deque\<int\> | 历史人数采样（绘制趋势图） |

### 2.3 AI 模型指针（第 39-40 行）

```cpp
cv::dnn::Net* g_net=nullptr;     // MobileNet SSD（人体）
cv::dnn::Net* face_net=nullptr;  // SSD Face Detector（人脸）
```

两个模型共存：face_net 优先使用，若文件不存在则 fallback 到 g_net。

### 2.4 detect() 检测函数（第 42-60 行）

```cpp
std::vector<cv::Rect> detect(cv::Mat& f){
    // 分支1：人脸检测（高精度）
    if(face_net){
        cv::Mat b=cv::dnn::blobFromImage(f,1.0,cv::Size(300,300),
            cv::Scalar(104,177,123),false,false);
        // 输出格式：[1,1,N,7] → image_id, label, confidence, x1, y1, x2, y2
        for(int i=0;i<N;i++){float* p=(float*)d.ptr()+i*7;
            if(p[2]>0.5){ ... }} // 置信度阈值 0.5
    }
    // 分支2：人体检测（后备）
    else {
        cv::Mat b=cv::dnn::blobFromImage(f,0.007843,cv::Size(300,300),127.5);
        for(int i=0;i<N;i++){float* p=(float*)d.ptr()+i*7;
            if(p[2]>0.5&&(int)p[1]==15){ ... }} // class_id=15=person
    }
}
```

**关键细节**：
- 人脸检测用 `blobFromImage(1.0, 300x300, (104,177,123))` —— OpenCV SSD Face 标准预处理
- 人体检测用 `blobFromImage(0.007843, 300x300, 127.5)` —— MobileNet SSD 标准预处理
- 输出格式相同：`[N, 7]` 其中 `[2]=confidence, [3-6]=x1,y1,x2,y2`
- 因为 OpenCV 4.5.4 的 `at<float>()` 不支持 4 维索引，所以用 `(float*)d.ptr()+i*7` 指针偏移

### 2.5 LoginDialog 登录对话框（第 62-87 行）

```cpp
class LoginDialog:public QDialog{
    Q_OBJECT  // MOC 元对象宏
    LoginDialog(QWidget* p=nullptr):QDialog(p){
        setStyleSheet("QDialog{background:#0a120e;}..."); // 暗绿色登录页
        pass_->setEchoMode(QLineEdit::Password);           // 密码遮罩
        connect(pass_,&QLineEdit::returnPressed,this,&LoginDialog::check); // 回车登录
    }
    void check(){
        if(user_->text()==ADMIN_USER&&pass_->text()==ADMIN_PASS)accept();
        else{QMessageBox::warning(...);}  // 失败清空密码框并聚焦
    }
};
```

**关键细节**：
- `Q_OBJECT` 宏是 Qt MOC（元对象编译器）的信号槽机制入口，必须配合 `#include "smartguard.moc"`
- `setEchoMode(Password)` 将密码显示为圆点
- `returnPressed` 信号绑定使得按回车即可触发登录

### 2.6 SmartGuard 主窗口构造（第 89-179 行）

#### 2.6.1 全局样式表（第 94-97 行）

```css
QMainWindow{background:#0d1117;}      /* GitHub Dark 底色 */
QPushButton{background:#21262d;...}    /* 按钮卡片灰 */
QPushButton:hover{border-color:#58a6ff;} /* 悬停蓝色边框 */
QTextEdit{color:#7ee787;...}           /* 终端绿文字 */
```

#### 2.6.2 左侧视频区（第 102-121 行）

```
┌─ QVBoxLayout (left)
│  ├─ video_ (QLabel, scaledContents=true)  ← 视频主显示区
│  └─ bar (QHBoxLayout)                      ← 状态栏
│     ├─ blink_dot_  LIVE                   ← 绿色闪烁灯
│     ├─ stream_dot_ RTMP                   ← 橙色推流灯
│     ├─ onenet_dot_ OneNET                 ← 青色 OneNET 灯
│     ├─ status_                            ← 状态文字
│     └─ clock_label_                       ← 时钟
```

`setScaledContents(true)` 使 QLabel 自适应填充，视频等比例拉伸。

#### 2.6.3 右侧数据面板（第 123-150 行）

```
┌─ QVBoxLayout (right)
│  ├─ title "SmartGuard"           ← 标题
│  ├─ card_ (QFrame)              ← 人数卡片
│  │  └─ num_ "0"                 ← 大号数字
│  ├─ info (QFrame)               ← 信息行
│  │  ├─ peak_label_              ← 峰值+停留时间
│  │  └─ time_label_              ← 更新时间
│  ├─ chart_ (QTextEdit)          ← 趋势图
│  └─ log_ (QTextEdit)            ← 事件日志
```

#### 2.6.4 ffmpeg 推流管道（第 152-161 行）

```cpp
ffmpeg_=new QProcess(this);
QStringList fa;fa<<"-y"<<"-f"<<"rawvideo"<<"-vcodec"<<"rawvideo"
    <<"-s"<<"320x240"<<"-pix_fmt"<<"bgr24"<<"-r"<<"15"<<"-i"<<"-"
    <<"-c:v"<<"libx264"<<"-preset"<<"ultrafast"<<"-tune"<<"zerolatency"
    <<"-b:v"<<"600k"<<"-maxrate"<<"600k"<<"-bufsize"<<"1200k"
    <<"-g"<<"30"<<"-keyint_min"<<"30"<<"-an"<<"-f"<<"flv"
    <<"rtmp://&lt;YOUR_SRS_SERVER_IP&gt;/live/camera";
ffmpeg_->start("ffmpeg",fa);
stream_ok_=ffmpeg_->waitForStarted(3000);
```

**逐参数分析**：

| 参数 | 值 | 含义 |
|------|-----|------|
| `-f rawvideo` | 原始视频格式 | 输入格式 |
| `-pix_fmt bgr24` | BGR 24位 | OpenCV Mat 的默认格式 |
| `-s 320x240` | 320x240 | 推流分辨率（低分辨率节省带宽） |
| `-r 15` | 15 FPS | 期望帧率 |
| `-i -` | stdin | 从管道读取 |
| `-c:v libx264` | H.264 | 视频编码 |
| `-preset ultrafast` | 最快 | 延迟优先于压缩率 |
| `-tune zerolatency` | 零延迟 | 直播模式 |
| `-b:v 600k` | 600kbps | 视频码率 |
| `-g 30` | GOP=30 | 每30帧一个关键帧 |
| `-an` | 无音频 | 摄像头没有麦克风 |
| `-f flv` | FLV 封装 | SRS HTTP-FLV 需要 |

#### 2.6.5 定时器体系（第 163-176 行）

| 定时器 | 间隔 | 功能 |
|--------|------|------|
| `blink_timer_` | 800ms | LIVE 绿灯闪烁 |
| `ct` | 1000ms | 右上角时钟刷新 |
| `ot` | 3000ms | OneNET 在线状态检测 |
| `timer_` | 66ms | **主循环** tick() ~15 FPS |

### 2.7 tick() 主循环（第 182-238 行）

这是系统的核心驱动函数，每 66ms 执行一次。

```
1. cam_->getFrame(f)           采集 640x480 BGR 帧
2. cv::resize → 320x240        缩小后写入 ffmpeg stdin（推流）
3. if(aic++%3==0):            每3帧跑一次AI（5 FPS）
     detect(f) → 框出人脸
4. 更新时间戳 + 停留计时
5. cv::cvtColor BGR→RGB       OpenCV → Qt 颜色转换
6. QImage → QPixmap           设置到 video_ QLabel
7. 更新 UI:
     - num_ 数字颜色（蓝/红/灰）
     - card_ 告警红色闪烁背景
     - peak_label_ 峰值+停留
8. 每30秒写入 history deque
9. 人数变化写 log_
10. 写 /tmp/seat_state.json   OneNET 数据源
11. 更新推流指示灯 (stream_dot_)
```

**告警逻辑（第 208-220 行）**：
```cpp
bool alarm=timing_val>30;           // 停留超过30秒
num_->setText(alarm?"!":...);       // 显示感叹号
if(alarm && now-last_blink>0.5){alarm_blink=!alarm_blink;} // 0.5秒切换
card_->setStyleSheet(background: alarm?(blink?"#3d1111":"#1a0a0a"):"#161b22");
```

### 2.8 closeEvent 清理（第 240-243 行）

```cpp
void closeEvent(QCloseEvent* e){
    if(ffmpeg_){ffmpeg_->closeWriteChannel();ffmpeg_->waitForFinished(2000);}
    if(cam_)cam_->stop();e->accept();
}
```

先关闭 ffmpeg 写通道（发送 EOF 让 ffmpeg 正常退出），等待 2 秒后释放摄像头。

### 2.9 main() 入口（第 250-267 行）

```cpp
// 1. 加载 AI 模型
face_net=new cv::dnn::Net(cv::dnn::readNetFromCaffe(...)); // 优先
g_net=new cv::dnn::Net(cv::dnn::readNetFromCaffe(...));    // 后备

// 2. 登录验证
LoginDialog login; if(login.exec()!=QDialog::Accepted)return 0;

// 3. 打开摄像头
V4L2Camera cam("/dev/video8",640,480);
if(!cam.init()||!cam.start()){...}

// 4. 进入主循环
SmartGuard w(&cam); w.show();
return app.exec();
```

`#include "smartguard.moc"` 是 MOC 生成的文件，包含 Q_OBJECT 的信号槽元数据实现。

---

## 3. v4l2_camera.cpp/h 分析

这是从 `/opt/v4l2_opencv_demo/` 复制来的 V4L2 摄像头驱动类。

**核心 API**：
- `V4L2Camera(device, width, height)` — 构造函数
- `init()` — 打开设备、设置格式、请求缓冲区
- `start()` — 启动流式传输
- `getFrame(Mat&)` — 获取一帧 BGR 图像
- `stop()` — 停止流

**技术要点**：
- 使用 Linux V4L2 API：`VIDIOC_S_FMT`, `VIDIOC_REQBUFS`, `VIDIOC_STREAMON`
- 处理 YUV420M（三平面）格式，软件转换为 BGR
- 4 个 DMA 缓冲区轮转

**为什么 ffmpeg 不能直接用**：OV13850 在 Allwinner VIN 框架下创建的是 `/dev/video8` 输出节点，格式为 YUV420M（三平面），ffmpeg 的 v4l2 模块不支持此格式。只有通过 V4L2 API 自定义读取才能获取帧数据。

---

## 4. mqtt_client.py 分析

### 4.1 架构

```python
# 配置
BROKER = "183.230.40.96"   # OneNET 公网 MQTT 地址
PORT = 1883                # 非 TLS 端口
TOPIC = "$sys/&lt;YOUR_PRODUCT_ID&gt;/&lt;YOUR_DEVICE_NAME&gt;/thing/property/post"
```

### 4.2 核心循环

```python
while running:
    token = generate_token()  # 动态生成 HMAC-SHA1 Token（防过期）
    client = mqtt.Client(client_id=DEVICE_NAME, protocol=MQTTv311)
    client.username_pw_set(PRODUCT_ID, token)
    client.connect(BROKER, PORT, keepalive=120)
    
    while running and connected:
        seat_val, timer_val, person_val = get_seat_state()  # 读 /tmp/seat_state.json
        payload = json.dumps({"id":..., "version":"1.0", "params":{...}})
        client.publish(TOPIC, payload, qos=1)
        time.sleep(10)
```

### 4.3 数据格式（OneJSON）

```json
{
  "id": "1712345678",
  "version": "1.0",
  "params": {
    "online_status": {"value": 1, "time": 1712345678000},
    "person_count":  {"value": 3, "time": 1712345678000},
    "seat_status":   {"value": 1, "time": 1712345678000},
    "timing":        {"value": 45, "time": 1712345678000}
  }
}
```

**关键**：`time` 字段是毫秒级时间戳，OneNET 用于属性值的时间序列排序。

### 4.4 回复监控

```python
client.subscribe(REPLY_TOPIC, qos=1)

def on_message(c, u, msg):
    data = json.loads(msg.payload)
    print(f"code={data.get('code')}")  # 200=成功, 2306=标识符不存在
```

这使得服务端返回立刻可见，方便调试（之前通过此机制发现了 `temperature` 标识符未定义的问题）。

---

## 5. onetoken.py 分析

```python
def generate_token():
    res = f"products/{PRODUCT_ID}/devices/{DEVICE_NAME}"
    et = int(time.time()) + 3600          # 当前时间 + 1小时
    
    key_bytes = base64.b64decode(DEVICE_KEY)
    sign_string = f"{et}\n{METHOD}\n{res}\n{VERSION}"  # sha1签名原文
    signature = hmac.new(key_bytes, sign_string.encode(), hashlib.sha1).digest()
    sign_b64 = base64.b64encode(signature).decode()
    
    token = f"version={VERSION}&res={URLEncode(res)}&et={et}&method={METHOD}&sign={URLEncode(sign_b64)}"
    return token
```

**Token 结构示例**：
```
version=2018-10-31
&res=products%2F&lt;YOUR_PRODUCT_ID&gt;%2Fdevices%2F&lt;YOUR_DEVICE_NAME&gt;
&et=1781175000
&method=sha1
&sign=Aq6l6A%2BGj20wkmZjU0Rs2qY5pug%3D
```

**安全机制**：
- 设备密钥 `VmY3N...` 以 Base64 存储在代码中
- 每次连接时动态生成 1 小时有效期的签名
- Python 内置的 `hmac` + `hashlib` 实现，无第三方依赖
- 签名使用 HMAC-SHA1（OneNET 平台指定算法）

---

## 6. Makefile_smart 分析

```makefile
CXX = g++
CXXFLAGS = -O2 -Wall -Wno-misleading-indentation $(shell pkg-config --cflags Qt5Widgets opencv4)
LDFLAGS = $(shell pkg-config --libs Qt5Widgets opencv4)

OBJS = smartguard.o v4l2_camera.o
TARGET = smartguard

smartguard.moc: smartguard.cpp        # MOC 预处理
    moc smartguard.cpp -o smartguard.moc

$(TARGET): $(OBJS)
    $(CXX) $(OBJS) $(LDFLAGS) -o $(TARGET)
```

**关键点**：
- `-O2` 优化级别（平衡速度和编译时间）
- `-Wno-misleading-indentation` 抑制缩进误导警告
- `pkg-config` 自动获取 Qt5 和 OpenCV 的头文件/库路径
- MOC 步骤是必须的——Qt 的信号槽语法需要预编译为 C++

---

## 7. 系统数据流

```
摄像头 OV13850 (2112x1568@30fps)
  │
  ▼
Allwinner VIN 框架
  │
  ▼
/dev/video8 (V4L2 设备节点)
  │
  ▼
v4l2_camera.cpp (YUV420M → BGR, 640x480)
  │
  ├─────────────────────────────┐
  ▼                             ▼
tick() @ 66ms                  ffmpeg stdin
  │                             │
  ├─ AI detect (每3帧)          │ cv::resize 640→320
  │  ├─ FaceNet (5 FPS)        │ H.264 ultrafast
  │  └─ MobileNet SSD (备用)    │
  │                             ▼
  ├─ Qt UI 更新                rtmp://&lt;YOUR_SRS_SERVER_IP&gt;/live/camera
  │  ├─ 视频显示                   │
  │  ├─ 人数/计时                    ▼
  │  └─ 告警状态                   SRS 服务器
  │                                 │
  ├─ /tmp/seat_state.json          ▼
  │                             HTTP-FLV
  ▼                             http://:8080/live/camera.flv
mqtt_client.py @ 10s                │
  │                                 ▼
  ▼                             微信小程序 live-player
OneNET MQTT (183.230.40.96:1883)
  │
  ▼
微信小程序 HTTP API
```

---

## 8. OneNET 对接详解

### 8.1 连接参数

| 参数 | 值 | 来源 |
|------|-----|------|
| Product ID | `&lt;YOUR_PRODUCT_ID&gt;` | OneNET 控制台创建产品时分配 |
| Device Name | `&lt;YOUR_DEVICE_NAME&gt;` | 设备注册时自定义 |
| Device Key | `&lt;YOUR_DEVICE_KEY&gt;` | 设备详情页 |
| Access Key | `&lt;YOUR_ACCESS_KEY&gt;` | 产品概况页 |
| MQTT Broker | `183.230.40.96:1883` | OneNET 标准 MQTT 接入点 |

### 8.2 物模型属性

| 标识符 | 类型 | 值域 | 说明 |
|--------|------|------|------|
| `online_status` | enum | 0/1 | 设备在线状态 |
| `person_count` | int32 | 0-99 | 当前人数 |
| `seat_status` | enum | 0-3 | 空闲/占用/置物/违规 |
| `timing` | int32 | 0-9999 | 停留计时(秒) |

### 8.3 数据上报主题

```
$sys/&lt;YOUR_PRODUCT_ID&gt;/&lt;YOUR_DEVICE_NAME&gt;/thing/property/post
```

响应主题：
```
$sys/&lt;YOUR_PRODUCT_ID&gt;/&lt;YOUR_DEVICE_NAME&gt;/thing/property/post/reply
```

---

## 9. SRS 视频推流详解

### 9.1 管道架构

```
SmartGuard (Qt C++)
  │
  ▼ cv::Mat (BGR, 640x480)
  │
  ▼ cv::resize → 320x240
  │
  ▼ QProcess::write() → ffmpeg stdin
  │
  ▼ ffmpeg (子进程)
  ├─ rawvideo → H.264
  ├─ ultrafast + zerolatency
  └─ RTMP push → &lt;YOUR_SRS_SERVER_IP&gt;:1935
```

### 9.2 为什么用 stdin 管道而不是 IPC

1. **共享内存**：QProcess 管道是内核管理的，动态分配，不需要预先分配固定大小
2. **背压处理**：ffmpeg 读取速度 < 写入速度时，管道缓冲区自动阻塞写端
3. **进程隔离**：ffmpeg 崩溃不影响主 Qt 进程
4. **零序列化**：原始 BGR 字节直接写入，无需 JSON/Protobuf

### 9.3 SRS 服务器信息

| 参数 | 值 |
|------|-----|
| RTMP | `rtmp://&lt;YOUR_SRS_SERVER_IP&gt;:1935/live/camera` |
| HTTP-FLV | `http://&lt;YOUR_SRS_SERVER_IP&gt;:8080/live/camera.flv` |
| HLS | `http://&lt;YOUR_SRS_SERVER_IP&gt;:8080/live/camera.m3u8` |

---

## 10. 性能优化分析

### 10.1 帧率解耦

```
主循环: 66ms (~15 FPS)
├── 推流: 每帧都推（15 FPS）
└── AI:   每3帧一次（5 FPS）
```

AI 推理在 ARM Cortex-A55 上需要约 200ms，如果每帧都跑会把帧率拖到 2-3 FPS。通过 3:1 的推流:AI 比例，保持了视频流畅，同时人数检测基本实时。

### 10.2 分辨率策略

| 用途 | 分辨率 | 原因 |
|------|--------|------|
| 摄像头采集 | 640x480 | AI 检测需要足够细节 |
| Qt 显示 | 640x480 | setScaledContents 自适应 |
| 推流 | 320x240 | 节省带宽，`cv::resize` 开销小于直接推送 640x480 |

### 10.3 推流延迟优化

| 措施 | 效果 |
|------|------|
| `-preset ultrafast` | 编码速度 > 压缩率，CPU 耗时最小 |
| `-tune zerolatency` | 禁用 B 帧，消除帧重排延迟 |
| `-g 30` (2秒 GOP) | 关键帧间隔短，首屏加载快 |
| `-b:v 600k` | 320x240 下画质足够，码率可控 |

---

## 附录 A：编译依赖

```bash
apt install qtbase5-dev libopencv-dev g++ make
pip3 install paho-mqtt
```

## 附录 B：调试命令

```bash
# 检查摄像头
ls /dev/video8 && v4l2-ctl --list-formats -d /dev/video8

# 检查推流
curl http://&lt;YOUR_SRS_SERVER_IP&gt;:1985/api/v1/clients

# 检查 OneNET
journalctl -u onenet -f

# 查看状态文件
cat /tmp/seat_state.json
```

## 附录 C：已知局限

1. OV13850 摄像头通过 VIN 框架暴露，ffmpeg/v4l2-ctl 无法直接读取，必须通过 v4l2_camera 自定义驱动
2. AI 推理在 CPU 上运行，帧率受限于 ARM A55 性能（5 FPS 人脸检测）
3. 多人检测时，人脸检测器对侧面/遮挡人脸可能漏检
4. OneNET HTTP API 查询需要 Access Key，且端点返回 500（建议用 MQTT 订阅代替）
