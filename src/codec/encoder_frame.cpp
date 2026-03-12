/**
 * @file encoder_frame.cpp
 * @brief Encoder frame implementation
 */

#include "minirtc/codec/encoder_frame.h"

namespace minirtc {

// ===== RawFrameImpl =====

RawFrameImpl::RawFrameImpl() 
    : media_type_(MediaType::kAudio) {
}

RawFrameImpl::RawFrameImpl(const VideoFrameInfo& info)
    : media_type_(MediaType::kVideo), video_info_(info) {
}

RawFrameImpl::RawFrameImpl(const AudioFrameInfo& info)
    : media_type_(MediaType::kAudio), audio_info_(info) {
}

const uint8_t* RawFrameImpl::GetPlaneData(int plane) const {
  if (plane >= 0 && plane < static_cast<int>(planes_.size())) {
    return planes_[plane];
  }
  return nullptr;
}

int RawFrameImpl::GetPlaneSize(int plane) const {
  if (plane >= 0 && plane < static_cast<int>(plane_sizes_.size())) {
    return static_cast<int>(plane_sizes_[plane]);
  }
  return 0;
}

void RawFrameImpl::SetDataRef(uint8_t* data, size_t size,
                              std::function<void(uint8_t*)> releaser) {
  data_.clear();
  data_.shrink_to_fit();
  planes_.clear();
  plane_sizes_.clear();
  
  releaser_ = releaser;
  // Note: This is a simplified implementation
  // In real zero-copy scenario, we'd store the pointer differently
  (void)data;
  (void)size;
}

RawFrame::Ptr RawFrameImpl::Clone() const {
  auto clone = std::make_shared<RawFrameImpl>();
  clone->media_type_ = media_type_;
  clone->video_info_ = video_info_;
  clone->audio_info_ = audio_info_;
  clone->data_ = data_;
  clone->timestamp_us_ = timestamp_us_;
  
  // Clone plane data references
  for (size_t i = 0; i < planes_.size(); ++i) {
    if (planes_[i] && i < data_.size()) {
      clone->planes_.push_back(clone->data_.data() + (planes_[i] - data_.data()));
    } else {
      clone->planes_.push_back(nullptr);
    }
  }
  clone->plane_sizes_ = plane_sizes_;
  
  return clone;
}

void RawFrameImpl::SetVideoInfo(const VideoFrameInfo& info) {
  media_type_ = MediaType::kVideo;
  video_info_ = info;
  timestamp_us_ = info.timestamp_us;  // Sync timestamp_us_ with video_info_
}

void RawFrameImpl::SetAudioInfo(const AudioFrameInfo& info) {
  media_type_ = MediaType::kAudio;
  audio_info_ = info;
}

void RawFrameImpl::SetPlaneData(int plane, const uint8_t* data, size_t size) {
  if (plane >= 0) {
    while (static_cast<size_t>(plane) >= planes_.size()) {
      planes_.push_back(nullptr);
      plane_sizes_.push_back(0);
    }
    // This is simplified - real impl would track offsets properly
    (void)data;
    plane_sizes_[plane] = size;
  }
}

void RawFrameImpl::SetData(const uint8_t* data, size_t size) {
  data_.assign(data, data + size);
}

// ===== EncodedFrameImpl =====

EncodedFrameImpl::EncodedFrameImpl() 
    : media_type_(MediaType::kAudio) {
}

EncodedFrameImpl::EncodedFrameImpl(MediaType media_type)
    : media_type_(media_type) {
}

void EncodedFrameImpl::AddNALUnit(const uint8_t* data, size_t size) {
  nal_units_.insert(nal_units_.end(), data, data + size);
}

void EncodedFrameImpl::SetDataRef(uint8_t* data, size_t size,
                                  std::function<void(uint8_t*)> releaser) {
  data_.clear();
  data_.shrink_to_fit();
  releaser_ = releaser;
  // Note: This is a simplified implementation
  (void)data;
  (void)size;
}

EncodedFrame::Ptr EncodedFrameImpl::Clone() const {
  auto clone = std::make_shared<EncodedFrameImpl>();
  clone->media_type_ = media_type_;
  clone->data_ = data_;
  clone->nal_units_ = nal_units_;
  clone->keyframe_ = keyframe_;
  clone->timestamp_us_ = timestamp_us_;
  clone->frame_number_ = frame_number_;
  clone->ssrc_ = ssrc_;
  clone->seq_num_ = seq_num_;
  clone->rtp_timestamp_ = rtp_timestamp_;
  clone->payload_type_ = payload_type_;
  return clone;
}

bool EncodedFrameImpl::ReserveBuffer(size_t size) {
  try {
    data_.reserve(size);
    return true;
  } catch (...) {
    return false;
  }
}

void EncodedFrameImpl::SetData(const uint8_t* data, size_t size) {
  data_.assign(data, data + size);
}

}  // namespace minirtc
