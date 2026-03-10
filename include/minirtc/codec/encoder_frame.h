/**
 * @file encoder_frame.h
 * @brief MiniRTC encoder frame interfaces
 */

#ifndef MINIRTC_ENCODER_FRAME_H_
#define MINIRTC_ENCODER_FRAME_H_

#include "codec_types.h"
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace minirtc {

/**
 * @brief Raw unencoded frame interface
 * 
 * Supports zero-copy views to avoid unnecessary data copies
 */
class RawFrame : public std::enable_shared_from_this<RawFrame> {
 public:
  using Ptr = std::shared_ptr<RawFrame>;
  
  virtual ~RawFrame() = default;
  
  // ===== Data Access =====
  
  /// Get video frame info
  virtual const VideoFrameInfo& GetVideoInfo() const = 0;
  
  /// Get audio frame info
  virtual const AudioFrameInfo& GetAudioInfo() const = 0;
  
  /// Get data pointer (Y plane or audio samples)
  virtual const uint8_t* GetData() const = 0;
  
  /// Get data size
  virtual size_t GetSize() const = 0;
  
  /// Get plane data (video)
  virtual const uint8_t* GetPlaneData(int plane) const = 0;
  virtual int GetPlaneSize(int plane) const = 0;
  
  // ===== Data Management =====
  
  /// Set data reference (zero-copy)
  virtual void SetDataRef(uint8_t* data, size_t size, 
                         std::function<void(uint8_t*)> releaser = nullptr) = 0;
  
  /// Copy data
  virtual Ptr Clone() const = 0;
  
  // ===== Timestamp =====
  
  /// Get timestamp (microseconds)
  virtual uint64_t GetTimestampUs() const = 0;
  
  /// Set timestamp
  virtual void SetTimestampUs(uint64_t ts) = 0;
  
  // ===== Media Type =====
  
  /// Get media type
  virtual MediaType GetMediaType() const = 0;
};

/**
 * @brief Encoded frame interface
 */
class EncodedFrame : public std::enable_shared_from_this<EncodedFrame> {
 public:
  using Ptr = std::shared_ptr<EncodedFrame>;
  
  virtual ~EncodedFrame() = default;
  
  // ===== Basic Info =====
  
  /// Get data pointer
  virtual const uint8_t* GetData() const = 0;
  
  /// Get data size
  virtual size_t GetSize() const = 0;
  
  /// Is keyframe
  virtual bool IsKeyframe() const = 0;
  
  /// Get timestamp (microseconds)
  virtual uint64_t GetTimestampUs() const = 0;
  
  /// Get frame number
  virtual uint32_t GetFrameNumber() const = 0;
  
  // ===== RTP Related Info =====
  
  /// Get SSRC
  virtual uint32_t GetSSRC() const = 0;
  
  /// Set SSRC
  virtual void SetSSRC(uint32_t ssrc) = 0;
  
  /// Get RTP sequence number
  virtual uint16_t GetSeqNum() const = 0;
  
  /// Set RTP sequence number
  virtual void SetSeqNum(uint16_t seq) = 0;
  
  /// Get RTP timestamp
  virtual uint32_t GetRtpTimestamp() const = 0;
  
  /// Set RTP timestamp
  virtual void SetRtpTimestamp(uint32_t ts) = 0;
  
  /// Get payload type
  virtual uint8_t GetPayloadType() const = 0;
  
  /// Set payload type
  virtual void SetPayloadType(uint8_t pt) = 0;
  
  // ===== NAL Units (H.264) =====
  
  /// Get NAL unit list
  virtual const std::vector<uint8_t>& GetNALUnits() const = 0;
  
  /// Add NAL unit
  virtual void AddNALUnit(const uint8_t* data, size_t size) = 0;
  
  // ===== Data Management =====
  
  /// Set data reference (zero-copy)
  virtual void SetDataRef(uint8_t* data, size_t size,
                         std::function<void(uint8_t*)> releaser = nullptr) = 0;
  
  /// Copy data
  virtual Ptr Clone() const = 0;
  
  // ===== Buffer Management =====
  
  /// Reserve buffer space
  virtual bool ReserveBuffer(size_t size) = 0;
  
  /// Get reserved buffer size
  virtual size_t GetBufferCapacity() const = 0;
  
  // ===== Media Type =====
  
  /// Get media type
  virtual MediaType GetMediaType() const = 0;
};

// ===== Concrete Implementations =====

/**
 * @brief Concrete raw frame implementation
 */
class RawFrameImpl : public RawFrame {
 public:
  RawFrameImpl();
  explicit RawFrameImpl(const VideoFrameInfo& info);
  explicit RawFrameImpl(const AudioFrameInfo& info);
  
  // RawFrame interface
  const VideoFrameInfo& GetVideoInfo() const override { return video_info_; }
  const AudioFrameInfo& GetAudioInfo() const override { return audio_info_; }
  const uint8_t* GetData() const override { return data_.data(); }
  size_t GetSize() const override { return data_.size(); }
  const uint8_t* GetPlaneData(int plane) const override;
  int GetPlaneSize(int plane) const override;
  
  void SetDataRef(uint8_t* data, size_t size,
                  std::function<void(uint8_t*)> releaser = nullptr) override;
  Ptr Clone() const override;
  
  uint64_t GetTimestampUs() const override { return timestamp_us_; }
  void SetTimestampUs(uint64_t ts) override { timestamp_us_ = ts; }
  MediaType GetMediaType() const override { return media_type_; }
  
  // Video helpers
  void SetVideoInfo(const VideoFrameInfo& info);
  void SetPlaneData(int plane, const uint8_t* data, size_t size);
  
  // Audio helpers
  void SetAudioInfo(const AudioFrameInfo& info);
  
  // Data access
  std::vector<uint8_t>& GetMutableData() { return data_; }
  void SetData(const uint8_t* data, size_t size);
  
 private:
  MediaType media_type_;
  VideoFrameInfo video_info_;
  AudioFrameInfo audio_info_;
  std::vector<uint8_t> data_;
  std::vector<uint8_t*> planes_;
  std::vector<size_t> plane_sizes_;
  std::function<void(uint8_t*)> releaser_;
  uint64_t timestamp_us_ = 0;
};

/**
 * @brief Concrete encoded frame implementation
 */
class EncodedFrameImpl : public EncodedFrame {
 public:
  EncodedFrameImpl();
  explicit EncodedFrameImpl(MediaType media_type);
  
  // EncodedFrame interface
  const uint8_t* GetData() const override { return data_.data(); }
  size_t GetSize() const override { return data_.size(); }
  bool IsKeyframe() const override { return keyframe_; }
  uint64_t GetTimestampUs() const override { return timestamp_us_; }
  uint32_t GetFrameNumber() const override { return frame_number_; }
  
  uint32_t GetSSRC() const override { return ssrc_; }
  void SetSSRC(uint32_t ssrc) override { ssrc_ = ssrc; }
  uint16_t GetSeqNum() const override { return seq_num_; }
  void SetSeqNum(uint16_t seq) override { seq_num_ = seq; }
  uint32_t GetRtpTimestamp() const override { return rtp_timestamp_; }
  void SetRtpTimestamp(uint32_t ts) override { rtp_timestamp_ = ts; }
  uint8_t GetPayloadType() const override { return payload_type_; }
  void SetPayloadType(uint8_t pt) override { payload_type_ = pt; }
  
  const std::vector<uint8_t>& GetNALUnits() const override { return nal_units_; }
  void AddNALUnit(const uint8_t* data, size_t size) override;
  
  void SetDataRef(uint8_t* data, size_t size,
                 std::function<void(uint8_t*)> releaser = nullptr) override;
  Ptr Clone() const override;
  
  bool ReserveBuffer(size_t size) override;
  size_t GetBufferCapacity() const override { return data_.capacity(); }
  
  MediaType GetMediaType() const override { return media_type_; }
  
  // Helpers
  void SetKeyframe(bool keyframe) { keyframe_ = keyframe; }
  void SetTimestampUs(uint64_t ts) { timestamp_us_ = ts; }
  void SetFrameNumber(uint32_t num) { frame_number_ = num; }
  std::vector<uint8_t>& GetMutableData() { return data_; }
  void SetData(const uint8_t* data, size_t size);
  
 private:
  MediaType media_type_;
  std::vector<uint8_t> data_;
  std::vector<uint8_t> nal_units_;
  std::function<void(uint8_t*)> releaser_;
  
  bool keyframe_ = false;
  uint64_t timestamp_us_ = 0;
  uint32_t frame_number_ = 0;
  
  uint32_t ssrc_ = 0;
  uint16_t seq_num_ = 0;
  uint32_t rtp_timestamp_ = 0;
  uint8_t payload_type_ = 0;
};

}  // namespace minirtc

#endif  // MINIRTC_ENCODER_FRAME_H_
