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
* @file h264_codec_gtest.cpp
*
* Tests for the H.264 encoder and decoder.
*
* @author Alvaro J. Gaona
*/

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include "as2_video/h264_decoder.hpp"
#include "as2_video/h264_encoder.hpp"

namespace
{

constexpr int kWidth = 64;
constexpr int kHeight = 48;

// NAL unit types: the low 5 bits after each 00 00 01 start code.
std::set<int> nalTypes(const std::vector<uint8_t> & au)
{
  std::set<int> types;
  for (size_t i = 0; i + 3 < au.size(); ++i) {
    if (au[i] == 0 && au[i + 1] == 0 && au[i + 2] == 1) {
      types.insert(au[i + 3] & 0x1f);
    }
  }
  return types;
}

// A diagonal gradient that moves with each frame, channels = bytes per pixel.
std::vector<uint8_t> gradient(int frame, int channels)
{
  std::vector<uint8_t> pixels(kWidth * kHeight * channels);
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      for (int c = 0; c < channels; ++c) {
        pixels[(y * kWidth + x) * channels + c] = static_cast<uint8_t>(2 * x + y + frame + 40 * c);
      }
    }
  }
  return pixels;
}

as2_video::EncoderOptions options(const char * encoding = "bgr8", const char * encoder = "")
{
  as2_video::EncoderOptions o;
  o.encoder = encoder;
  o.width = kWidth;
  o.height = kHeight;
  o.encoding = encoding;
  o.gop = 5;
  o.bitrate = 4'000'000;
  return o;
}

// Hardware encoders are asynchronous and lag further behind when flooded.
void pace() {std::this_thread::sleep_for(std::chrono::milliseconds(20));}

}  // namespace

// Runs against each encoder this machine has; empty is the automatic pick.
class EachEncoder : public ::testing::TestWithParam<const char *>
{
protected:
  std::unique_ptr<as2_video::H264Encoder> open(as2_video::EncoderOptions o)
  {
    o.encoder = GetParam();
    try {
      return std::make_unique<as2_video::H264Encoder>(o);
    } catch (const std::runtime_error &) {
      return nullptr;
    }
  }
};

TEST_P(EachEncoder, KeyframesCarrySpsAndPps)
{
  auto encoder_ptr = open(options());
  if (!encoder_ptr) {
    GTEST_SKIP() << GetParam() << " is not available";
  }
  auto & encoder = *encoder_ptr;
  int keyframes = 0;
  for (int i = 0; i < 20; ++i) {
    pace();
    for (const auto & frame : encoder.encode(gradient(i, 3).data(), kWidth * 3)) {
      const auto types = nalTypes(frame.data);
      if (types.count(5)) {
        EXPECT_TRUE(frame.keyframe);
        EXPECT_TRUE(types.count(7) && types.count(8)) << "SPS and PPS ride with every IDR";
        ++keyframes;
      }
    }
  }
  EXPECT_GE(keyframes, 3) << encoder.name();
}

TEST_P(EachEncoder, RequestedKeyframe)
{
  auto o = options();
  o.gop = 1000;
  auto encoder_ptr = open(o);
  if (!encoder_ptr) {
    GTEST_SKIP() << GetParam() << " is not available";
  }
  auto & encoder = *encoder_ptr;
  std::vector<as2_video::EncodedFrame> frames;
  for (int i = 0; i < 20; ++i) {
    pace();
    if (i == 10) {
      encoder.requestKeyframe();
    }
    for (auto & frame : encoder.encode(gradient(i, 3).data(), kWidth * 3)) {
      frames.push_back(std::move(frame));
    }
  }
  bool requested_keyframe = false;
  for (const auto & frame : frames) {
    EXPECT_EQ(frame.keyframe, frame.pts == 0 || frame.pts == 10) << "pts " << frame.pts;
    requested_keyframe |= frame.pts == 10 && nalTypes(frame.data).count(5);
  }
  EXPECT_TRUE(requested_keyframe) << encoder.name();
}

INSTANTIATE_TEST_SUITE_P(
  Encoders, EachEncoder,
  ::testing::Values("", "h264_videotoolbox", "h264_nvenc", "libx264", "libopenh264"),
  [](const auto & info) {return info.param[0] ? std::string(info.param) : std::string("auto");});

TEST(H264Encoder, RejectsBadOptions)
{
  auto odd = options();
  odd.width = 63;
  EXPECT_THROW(as2_video::H264Encoder{odd}, std::invalid_argument);
  EXPECT_THROW(as2_video::H264Encoder{options("16UC1")}, std::invalid_argument);
  auto missing = options();
  missing.encoder = "no_such_encoder";
  EXPECT_THROW(as2_video::H264Encoder{missing}, std::runtime_error);
  EXPECT_TRUE(as2_video::H264Encoder::supportsEncoding("rgb8"));
  EXPECT_FALSE(as2_video::H264Encoder::supportsEncoding("32FC1"));
}

class RoundTrip : public ::testing::TestWithParam<const char *> {};

TEST_P(RoundTrip, DecodesWhatWasEncoded)
{
  const std::string encoding = GetParam();
  const int channels = encoding == "mono8" ? 1 : encoding == "rgba8" ? 4 : 3;
  as2_video::H264Encoder encoder(options(encoding.c_str()));
  as2_video::H264Decoder decoder;

  int decoded = 0;
  for (int i = 0; i < 20; ++i) {
    pace();
    const auto input = gradient(i, channels);
    for (const auto & frame : encoder.encode(input.data(), kWidth * channels)) {
      for (const auto & image : decoder.decode(frame.data.data(), frame.data.size(), frame.pts)) {
        ASSERT_EQ(image.width, kWidth);
        ASSERT_EQ(image.height, kHeight);
        ASSERT_EQ(image.bgr.size(), static_cast<size_t>(kWidth * kHeight * 3));
        EXPECT_EQ(image.pts, frame.pts);
        // Compare one channel against the source, mapped through the input encoding.
        const auto source = gradient(static_cast<int>(image.pts), channels);
        const int src_channel = channels == 1 ? 0 : encoding[0] == 'r' ? 0 : 2;
        const int out_channel = channels == 1 ? 1 : 2;  // red, or green for gray
        double error = 0;
        for (int p = 0; p < kWidth * kHeight; ++p) {
          error += std::abs(
            image.bgr[p * 3 + out_channel] - source[p * channels + src_channel]);
        }
        EXPECT_LT(error / (kWidth * kHeight), 12.0) << encoder.name();
        ++decoded;
      }
    }
  }
  EXPECT_GE(decoded, 10) << encoder.name();
}

INSTANTIATE_TEST_SUITE_P(
  Encodings, RoundTrip, ::testing::Values("bgr8", "rgb8", "rgba8", "mono8"));

TEST(H264Decoder, DropsDataBeforeTheFirstKeyframe)
{
  as2_video::H264Encoder encoder(options());
  std::vector<as2_video::EncodedFrame> frames;
  for (int i = 0; i < 8; ++i) {
    pace();
    for (auto & frame : encoder.encode(gradient(i, 3).data(), kWidth * 3)) {
      frames.push_back(std::move(frame));
    }
  }
  // Join after the first keyframe: nothing decodes until the next one.
  as2_video::H264Decoder decoder;
  int64_t first = -1;
  for (const auto & frame : frames) {
    if (frame.pts == 0) {
      continue;
    }
    const auto images = decoder.decode(frame.data.data(), frame.data.size(), frame.pts);
    if (!images.empty() && first < 0) {
      first = images.front().pts;
    }
  }
  EXPECT_EQ(first, 5) << encoder.name();
}
