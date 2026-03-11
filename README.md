# MiniRTC

MiniRTC 是一个轻量级的实时通信（Real-Time Communication）库，专为嵌入式系统和资源受限环境设计。它提供了完整的音视频采集、编解码、传输和渲染能力。

## 特性

### 音视频采集与渲染
- **视频采集**：支持多种视频捕获设备，包括摄像头和屏幕捕获
- **音频采集**：支持麦克风音频采集，具备音量检测和静音检测功能
- **视频渲染**：支持多种像素格式的硬件/软件渲染
- **音频播放**：支持低延迟音频播放，具备缓冲和音量控制功能

### 音视频编解码
- **视频编解码器**：
  - H.264（支持硬件加速）
  - H.265/HEVC
  - VP8/VP9
  - AV1
- **音频编解码器**：
  - Opus（支持语音和音乐模式）
  - AAC
  - G.711（PCMU/PCMA）

### 网络传输
- **ICE**：完整的 Interactive Connectivity Establishment 实现
- **DTLS-SRTP**：安全的媒体传输加密
- **带宽估计**：基于GCC的带宽估计算法
- **拥塞控制**：自适应码率控制

### 抗丢包与纠错
- **NACK**：Negative Acknowledgment 重传请求
- **FEC**：前向纠错编码
- **Jitter Buffer**：自适应抖动缓冲
- **Packet Cache**：数据包缓存管理

### 其他特性
- **内存池**：高效的内存管理，减少内存碎片
- **模块化设计**：各模块独立，可按需组合
- **跨平台**：支持 Linux、macOS、Windows 等平台

## 构建要求

- CMake 3.15+
- C++17 编译器
- POSIX 兼容系统（Linux/macOS）

## 构建方法

### 1. 克隆项目

```bash
git clone https://github.com/your-repo/MiniRTC.git
cd MiniRTC
```

### 2. 创建构建目录

```bash
mkdir build && cd build
```

### 3. 配置构建

```bash
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF
```

构建选项：
- `BUILD_TESTS`：构建测试用例（默认 ON）
- `BUILD_EXAMPLES`：构建示例程序（默认 OFF）

### 4. 编译

```bash
cmake --build . -j$(nproc)
```

### 5. 安装（可选）

```bash
cmake --install .
```

## 测试方法

### 运行所有测试

构建完成后，在 build 目录下运行：

```bash
ctest --output-on-failure
```

### 运行特定测试

```bash
# 运行单元测试
./bin/minirtc_unit_test

# 运行带宽估计测试
./bin/test_bandwidth_estimator

# 运行Jitter Buffer测试
./bin/test_jitter_buffer

# 运行ICE测试
./bin/test_ice

# 运行DTLS-SRTP测试
./bin/test_dtls_srtp

# 运行编解码测试
./bin/test_codec
```

### 查看测试覆盖

测试用例位于 `test/` 目录：
- `test/test_*.cpp` - 各模块单元测试
- `test/integration/` - 集成测试
- `test/unit/` - 单元测试

## 项目结构

```
MiniRTC/
├── include/minirtc/     # 头文件
│   ├── codec/          # 编解码模块
│   └── transport/      # 传输模块
├── src/                # 源代码
├── test/               # 测试用例
├── doc/                # 文档
└── third_party/       # 第三方依赖
```

## 快速开始

See [Quick Start Guide](doc/QUICKSTART.md) for a simple voice/video call example.

## API 参考

See [API Reference](doc/API_REFERENCE.md) for detailed API documentation.

## 许可证

MIT License
