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
* @file h264_encoder.cpp
*
* H.264 encoder with hardware acceleration when available.
*
* @author Alvaro J. Gaona
*/

#include "as2_video/h264_encoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace as2_video
{
namespace
{

using Option = std::pair<const char *, const char *>;

struct Candidate
{
  const char * name;
  std::vector<Option> options;
};

// Hardware first. Each one fails to open where its device is missing.
const std::vector<Candidate> kCandidates = {
  {"h264_videotoolbox", {{"realtime", "1"}, {"prio_speed", "1"}, {"allow_sw", "0"}}},
  {"h264_nvenc", {{"preset", "p1"}, {"tune", "ull"}, {"zerolatency", "1"}, {"forced-idr", "1"}}},
  {"h264_v4l2m2m", {}},
  {"libx264", {{"preset", "ultrafast"}, {"tune", "zerolatency"}, {"forced-idr", "1"}}},
  {"libopenh264", {{"allow_skip_frames", "0"}}},
};

const std::map<std::string, AVPixelFormat> kPixelFormats = {
  {"bgr8", AV_PIX_FMT_BGR24},
  {"rgb8", AV_PIX_FMT_RGB24},
  {"bgra8", AV_PIX_FMT_BGRA},
  {"rgba8", AV_PIX_FMT_RGBA},
  {"mono8", AV_PIX_FMT_GRAY8},
  {"yuv422", AV_PIX_FMT_UYVY422},
  {"yuv422_yuy2", AV_PIX_FMT_YUYV422},
};

AVCodecContext * open(const Candidate & candidate, const EncoderOptions & o)
{
  const AVCodec * codec = avcodec_find_encoder_by_name(candidate.name);
  if (!codec) {
    return nullptr;
  }
  AVCodecContext * ctx = avcodec_alloc_context3(codec);
  ctx->width = o.width;
  ctx->height = o.height;
  ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  ctx->time_base = {1, o.fps};
  ctx->framerate = {o.fps, 1};
  ctx->gop_size = o.gop;
  ctx->max_b_frames = 0;
  ctx->bit_rate = o.bitrate;
  for (const auto & [key, value] : candidate.options) {
    av_opt_set(ctx->priv_data, key, value, 0);
  }
  if (avcodec_open2(ctx, codec, nullptr) < 0) {
    avcodec_free_context(&ctx);
  }
  return ctx;
}

}  // namespace

bool H264Encoder::supportsEncoding(const std::string & encoding)
{
  return kPixelFormats.count(encoding) > 0;
}

H264Encoder::H264Encoder(const EncoderOptions & o)
: options_(o)
{
  if (o.width <= 0 || o.height <= 0 || o.width % 2 || o.height % 2) {
    throw std::invalid_argument(
            "image size must be positive and even, got " + std::to_string(o.width) + "x" +
            std::to_string(o.height));
  }
  if (o.fps <= 0 || o.gop <= 0 || o.bitrate <= 0) {
    throw std::invalid_argument("fps, gop and bitrate must be positive");
  }
  const auto input = kPixelFormats.find(o.encoding);
  if (input == kPixelFormats.end()) {
    throw std::invalid_argument("unsupported encoding '" + o.encoding + "'");
  }

  // Hardware encoders that are not present log errors while being probed.
  av_log_set_level(AV_LOG_FATAL);
  for (const auto & candidate : kCandidates) {
    if (!o.encoder.empty() && o.encoder != candidate.name) {
      continue;
    }
    if ((ctx_ = open(candidate, o))) {
      name_ = candidate.name;
      break;
    }
  }
  if (!ctx_ && !o.encoder.empty() && (ctx_ = open({o.encoder.c_str(), {}}, o))) {
    name_ = o.encoder;
  }
  if (!ctx_) {
    throw std::runtime_error(
            "no H.264 encoder could be opened" + (o.encoder.empty() ? "" : " (" + o.encoder + ")"));
  }

  frame_ = av_frame_alloc();
  packet_ = av_packet_alloc();
  sws_ = sws_getContext(
    o.width, o.height, input->second, o.width, o.height, ctx_->pix_fmt,
    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  if (!frame_ || !packet_ || !sws_) {
    release();
    throw std::runtime_error("failed to allocate encoder buffers");
  }
  frame_->format = ctx_->pix_fmt;
  frame_->width = o.width;
  frame_->height = o.height;
  if (av_frame_get_buffer(frame_, 0) < 0) {
    release();
    throw std::runtime_error("failed to allocate encoder buffers");
  }
}

H264Encoder::~H264Encoder() {release();}

void H264Encoder::release()
{
  sws_freeContext(sws_);
  sws_ = nullptr;
  av_packet_free(&packet_);
  av_frame_free(&frame_);
  avcodec_free_context(&ctx_);
}

std::vector<EncodedFrame> H264Encoder::encode(const uint8_t * pixels, int stride)
{
  if (av_frame_make_writable(frame_) < 0) {
    throw std::runtime_error("encoder frame not writable");
  }
  sws_scale(sws_, &pixels, &stride, 0, ctx_->height, frame_->data, frame_->linesize);
  frame_->pts = pts_++;
  frame_->pict_type = keyframe_requested_ ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_NONE;
  keyframe_requested_ = false;
  if (avcodec_send_frame(ctx_, frame_) < 0) {
    throw std::runtime_error(name_ + ": send_frame failed");
  }

  std::vector<EncodedFrame> out;
  while (avcodec_receive_packet(ctx_, packet_) == 0) {
    EncodedFrame encoded;
    encoded.data.assign(packet_->data, packet_->data + packet_->size);
    encoded.pts = packet_->pts;
    encoded.keyframe = packet_->flags & AV_PKT_FLAG_KEY;
    out.push_back(std::move(encoded));
    av_packet_unref(packet_);
  }
  return out;
}

}  // namespace as2_video
