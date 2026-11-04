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
* @file h264_decoder.hpp
*
* H.264 decoder that outputs packed BGR frames.
*
* @author Alvaro J. Gaona
*/

#ifndef AS2_VIDEO__H264_DECODER_HPP_
#define AS2_VIDEO__H264_DECODER_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace as2_video
{

struct DecodedFrame
{
  int width = 0;
  int height = 0;
  // Packed BGR24, width * 3 bytes per row.
  std::vector<uint8_t> bgr;
  // The pts passed to decode() with the access unit this frame came from.
  int64_t pts = 0;
};

// H.264 Annex B access units in, BGR frames out. Data before the first keyframe is dropped.
class H264Decoder
{
public:
  H264Decoder();
  ~H264Decoder();
  H264Decoder(const H264Decoder &) = delete;
  H264Decoder & operator=(const H264Decoder &) = delete;

  std::vector<DecodedFrame> decode(const uint8_t * data, size_t size, int64_t pts);

private:
  void release();

  AVCodecContext * ctx_ = nullptr;
  AVFrame * frame_ = nullptr;
  AVPacket * packet_ = nullptr;
  SwsContext * sws_ = nullptr;
};

}  // namespace as2_video

#endif  // AS2_VIDEO__H264_DECODER_HPP_
