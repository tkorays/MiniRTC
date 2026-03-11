# MiniRTC 第二轮迭代计划

## 当前状态

已完成:
- 基础框架 (采集/播放/编解码/RTP传输)
- Stream/Track抽象
- JitterBuffer (透传/固定/自适应)
- H.264打包/组帧
- ICE模块
- DTLS/SRTP接口
- 带宽估计器
- 内存池
- PeerConnection
- 命令行Demo

## 需要补充

### 1. 完整Pipeline (P0)
- [ ] Demo中集成Opus编码
- [ ] Demo中集成H.264编码
- [ ] 端到端音视频传输

### 2. 媒体同步 (P1)
- [ ] RTP时间戳同步
- [ ] NTP时间映射
- [ ] 唇音同步

### 3. 集成测试 (P1)
- [ ] 音频Pipeline测试
- [ ] 视频Pipeline测试
- [ ] 完整E2E测试

### 4. 性能优化 (P2)
- [ ] 内存池完善
- [ ] 减少拷贝

## 时间安排
- 17:00-18:30: 完整Pipeline
- 18:30-19:00: 汇报
