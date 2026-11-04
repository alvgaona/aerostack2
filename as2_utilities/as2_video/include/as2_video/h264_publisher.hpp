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
* @file h264_publisher.hpp
*
* image_transport publisher plugin that streams images as H.264.
*
* @author Alvaro J. Gaona
*/

#ifndef AS2_VIDEO__H264_PUBLISHER_HPP_
#define AS2_VIDEO__H264_PUBLISHER_HPP_

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <image_transport/simple_publisher_plugin.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <sensor_msgs/msg/image.hpp>

#include "as2_video/h264_encoder.hpp"

namespace as2_video
{

// Publishes sensor_msgs/CompressedImage with format "h264" on <base_topic>/h264. The encoder
// starts on the first image, restarts when the image size or encoding changes, and only runs
// while the topic has subscribers. Each new subscriber triggers a keyframe.
//
// Parameters, under <base_topic>.h264 with the node namespace removed and '/' replaced by '.':
//   bitrate (int, 2000000), fps (int, 30), gop (int, = fps), encoder (string, "" = auto).
class H264Publisher
  : public image_transport::SimplePublisherPlugin<sensor_msgs::msg::CompressedImage>
{
public:
  H264Publisher();
  std::string getTransportName() const override {return "h264";}

protected:
#ifdef AS2_VIDEO_IMAGE_TRANSPORT_OPTIONS
  void advertiseImpl(
    rclcpp::Node * node, const std::string & base_topic, rmw_qos_profile_t custom_qos,
    rclcpp::PublisherOptions options) override;
#else
  void advertiseImpl(
    rclcpp::Node * node, const std::string & base_topic, rmw_qos_profile_t custom_qos) override;
#endif

  void publish(
    const sensor_msgs::msg::Image & image, const PublishFn & publish_fn) const override;

private:
  void declareParameters(rclcpp::Node * node, const std::string & base_topic);

  EncoderOptions options_;
  rclcpp::Logger logger_;

  mutable std::mutex mutex_;
  mutable std::unique_ptr<H264Encoder> encoder_;
  // Headers of the frames inside the encoder, by input frame index.
  mutable std::deque<std::pair<int64_t, std_msgs::msg::Header>> pending_;
  mutable int64_t next_pts_ = 0;
  mutable size_t subscribers_ = 0;
  mutable std::chrono::steady_clock::time_point last_image_;
  // Last image format the encoder failed to open for, so the error is logged once.
  mutable std::string failed_format_;
};

}  // namespace as2_video

#endif  // AS2_VIDEO__H264_PUBLISHER_HPP_
