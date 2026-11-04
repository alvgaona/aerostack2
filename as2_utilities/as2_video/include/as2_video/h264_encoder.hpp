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
* @file h264_encoder.hpp
*
* H.264 encoder with hardware acceleration when available.
*
* @author Alvaro J. Gaona
*/

#ifndef AS2_VIDEO__H264_ENCODER_HPP_
#define AS2_VIDEO__H264_ENCODER_HPP_

#include <cstdint>
#include <string>
#include <vector>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace as2_video
{

struct EncoderOptions
{
  int width = 0;
  int height = 0;
  // sensor_msgs/Image encoding of the input:
  // bgr8, rgb8, bgra8, rgba8, mono8, yuv422 (UYVY) or yuv422_yuy2.
  std::string encoding = "bgr8";
  int fps = 30;
  // Frames between keyframes.
  int gop = 30;
  int64_t bitrate = 2'000'000;
  // FFmpeg encoder name. Empty picks the first that opens: hardware first, then software.
  std::string encoder;
};

struct EncodedFrame
{
  // H.264 Annex B access unit.
  std::vector<uint8_t> data;
  // Index of the input frame this came from, counting from 0.
  int64_t pts = 0;
  bool keyframe = false;
};

// Raw frames in, H.264 access units out. No B-frames, and SPS/PPS ride with every keyframe,
// so a subscriber can join mid-stream and start decoding at the next keyframe.
class H264Encoder
{
public:
  explicit H264Encoder(const EncoderOptions & options);
  ~H264Encoder();
  H264Encoder(const H264Encoder &) = delete;
  H264Encoder & operator=(const H264Encoder &) = delete;

  // Returns the frames the encoder finished. Hardware encoders may lag a few frames behind.
  std::vector<EncodedFrame> encode(const uint8_t * pixels, int stride);

  // Makes the next frame a keyframe, so a new subscriber starts right away.
  void requestKeyframe() {keyframe_requested_ = true;}

  const std::string & name() const {return name_;}
  const EncoderOptions & options() const {return options_;}

  static bool supportsEncoding(const std::string & encoding);

private:
  void release();

  EncoderOptions options_;
  AVCodecContext * ctx_ = nullptr;
  AVFrame * frame_ = nullptr;
  AVPacket * packet_ = nullptr;
  SwsContext * sws_ = nullptr;
  std::string name_;
  int64_t pts_ = 0;
  bool keyframe_requested_ = false;
};

}  // namespace as2_video

#endif  // AS2_VIDEO__H264_ENCODER_HPP_
