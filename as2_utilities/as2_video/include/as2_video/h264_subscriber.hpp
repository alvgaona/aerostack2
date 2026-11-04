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
* @file h264_subscriber.hpp
*
* image_transport subscriber plugin that decodes an H.264 stream.
*
* @author Alvaro J. Gaona
*/

#ifndef AS2_VIDEO__H264_SUBSCRIBER_HPP_
#define AS2_VIDEO__H264_SUBSCRIBER_HPP_

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>

#include <image_transport/simple_subscriber_plugin.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>

#include "as2_video/h264_decoder.hpp"

namespace as2_video
{

// Subscribes to <base_topic>/h264 and hands bgr8 images to the callback, starting at the
// first keyframe.
class H264Subscriber
  : public image_transport::SimpleSubscriberPlugin<sensor_msgs::msg::CompressedImage>
{
public:
  std::string getTransportName() const override {return "h264";}

protected:
#ifndef AS2_VIDEO_IMAGE_TRANSPORT_OPTIONS
  // Humble's base class logs an error unless this overload is overridden.
  void subscribeImpl(
    rclcpp::Node * node, const std::string & base_topic, const Callback & callback,
    rmw_qos_profile_t custom_qos, rclcpp::SubscriptionOptions options) override
  {
    subscribeImplWithOptions(node, base_topic, callback, custom_qos, options);
  }
  using SimpleSubscriberPlugin::subscribeImpl;
#endif

  void internalCallback(
    const std::shared_ptr<const sensor_msgs::msg::CompressedImage> & message,
    const Callback & user_cb) override;

private:
  std::unique_ptr<H264Decoder> decoder_;
  std::deque<std::pair<int64_t, std_msgs::msg::Header>> pending_;
  int64_t next_pts_ = 0;
};

}  // namespace as2_video

#endif  // AS2_VIDEO__H264_SUBSCRIBER_HPP_
