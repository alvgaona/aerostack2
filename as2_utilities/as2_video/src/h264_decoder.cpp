// Copyright 2026 Universidad Politécnica de Madrid
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the Universidad Politécnica de Madrid nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
* @file h264_decoder.cpp
*
* H.264 decoder that outputs packed BGR frames.
*
* @author Alvaro J. Gaona
*/

#include "as2_video/h264_decoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace as2_video
{

H264Decoder::H264Decoder()
{
  av_log_set_level(AV_LOG_FATAL);
  const AVCodec * codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) {
    throw std::runtime_error("FFmpeg has no H.264 decoder");
  }
  ctx_ = avcodec_alloc_context3(codec);
  if (!ctx_) {
    throw std::runtime_error("failed to allocate the H.264 decoder");
  }
  // Output each frame as soon as its access unit arrives; frame threading would add latency.
  ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
  ctx_->thread_type = FF_THREAD_SLICE;
  frame_ = av_frame_alloc();
  packet_ = av_packet_alloc();
  if (!frame_ || !packet_ || avcodec_open2(ctx_, codec, nullptr) < 0) {
    release();
    throw std::runtime_error("failed to open the H.264 decoder");
  }
}

H264Decoder::~H264Decoder() {release();}

void H264Decoder::release()
{
  sws_freeContext(sws_);
  sws_ = nullptr;
  av_packet_free(&packet_);
  av_frame_free(&frame_);
  avcodec_free_context(&ctx_);
}

std::vector<DecodedFrame> H264Decoder::decode(const uint8_t * data, size_t size, int64_t pts)
{
  std::vector<DecodedFrame> out;
  // FFmpeg reads a few bytes past the end of its input, so copy into a padded packet.
  if (size == 0 || av_new_packet(packet_, static_cast<int>(size)) < 0) {
    return out;
  }
  std::memcpy(packet_->data, data, size);
  packet_->pts = pts;
  // Fails until the first keyframe brings SPS/PPS; those packets are dropped.
  const int sent = avcodec_send_packet(ctx_, packet_);
  av_packet_unref(packet_);
  if (sent < 0) {
    return out;
  }

  while (avcodec_receive_frame(ctx_, frame_) == 0) {
    const int width = frame_->width;
    const int height = frame_->height;
    sws_ = sws_getCachedContext(
      sws_, width, height, static_cast<AVPixelFormat>(frame_->format), width, height,
      AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_) {
      av_frame_unref(frame_);
      throw std::runtime_error("failed to set up the decoder color conversion");
    }
    DecodedFrame decoded;
    decoded.width = width;
    decoded.height = height;
    decoded.bgr.resize(static_cast<size_t>(width) * height * 3);
    uint8_t * dst[] = {decoded.bgr.data()};
    const int dst_stride[] = {width * 3};
    sws_scale(sws_, frame_->data, frame_->linesize, 0, height, dst, dst_stride);
    decoded.pts = frame_->pts != AV_NOPTS_VALUE ? frame_->pts : frame_->best_effort_timestamp;
    av_frame_unref(frame_);
    out.push_back(std::move(decoded));
  }
  return out;
}

}  // namespace as2_video
