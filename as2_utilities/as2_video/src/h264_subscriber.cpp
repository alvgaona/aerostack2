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
* @file h264_subscriber.cpp
*
* image_transport subscriber plugin that decodes an H.264 stream.
*
* @author Alvaro J. Gaona
*/

#include "as2_video/h264_subscriber.hpp"

#include <memory>
#include <utility>

#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/image.hpp>

namespace as2_video
{
namespace
{

// Headers kept for access units the decoder has not returned yet.
constexpr size_t kMaxPending = 64;

}  // namespace

void H264Subscriber::internalCallback(
  const std::shared_ptr<const sensor_msgs::msg::CompressedImage> & message,
  const Callback & user_cb)
{
  if (!decoder_) {
    decoder_ = std::make_unique<H264Decoder>();
  }
  pending_.emplace_back(next_pts_, message->header);
  if (pending_.size() > kMaxPending) {
    pending_.pop_front();
  }

  for (auto & frame : decoder_->decode(message->data.data(), message->data.size(), next_pts_++)) {
    while (!pending_.empty() && pending_.front().first < frame.pts) {
      pending_.pop_front();
    }
    if (pending_.empty() || pending_.front().first != frame.pts) {
      continue;
    }
    auto image = std::make_shared<sensor_msgs::msg::Image>();
    image->header = pending_.front().second;
    pending_.pop_front();
    image->width = frame.width;
    image->height = frame.height;
    image->encoding = sensor_msgs::image_encodings::BGR8;
    image->is_bigendian = false;
    image->step = frame.width * 3;
    image->data = std::move(frame.bgr);
    user_cb(image);
  }
}

}  // namespace as2_video
