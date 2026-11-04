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
* @file h264_publisher.cpp
*
* image_transport publisher plugin that streams images as H.264.
*
* @author Alvaro J. Gaona
*/

#include "as2_video/h264_publisher.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace as2_video
{
namespace
{

// A gap this long between images means nobody was subscribed in between, so the next
// subscriber may have missed the last keyframe.
constexpr double kGapSeconds = 0.5;

}  // namespace

H264Publisher::H264Publisher()
: logger_(rclcpp::get_logger("h264_publisher")) {}

#ifdef AS2_VIDEO_IMAGE_TRANSPORT_OPTIONS
void H264Publisher::advertiseImpl(
  rclcpp::Node * node, const std::string & base_topic, rmw_qos_profile_t custom_qos,
  rclcpp::PublisherOptions options)
{
  logger_ = node->get_logger().get_child("h264");
  declareParameters(node, base_topic);
  SimplePublisherPlugin::advertiseImpl(node, base_topic, custom_qos, options);
}
#else
void H264Publisher::advertiseImpl(
  rclcpp::Node * node, const std::string & base_topic, rmw_qos_profile_t custom_qos)
{
  logger_ = node->get_logger().get_child("h264");
  declareParameters(node, base_topic);
  SimplePublisherPlugin::advertiseImpl(node, base_topic, custom_qos);
}
#endif

void H264Publisher::declareParameters(rclcpp::Node * node, const std::string & base_topic)
{
  // /drone0/sensor_measurements/camera/image in namespace /drone0
  // -> sensor_measurements.camera.image.h264.<name>
  std::string prefix = base_topic;
  const std::string ns = node->get_effective_namespace();
  if (ns != "/" && prefix.rfind(ns + "/", 0) == 0) {
    prefix.erase(0, ns.size());
  }
  prefix.erase(0, prefix.find_first_not_of('/'));
  std::replace(prefix.begin(), prefix.end(), '/', '.');
  prefix += ".h264.";

  auto declare = [&](const std::string & name, auto default_value) {
    using T = decltype(default_value);
    if (node->has_parameter(prefix + name)) {
      return node->get_parameter(prefix + name).get_value<T>();
    }
    return node->declare_parameter<T>(prefix + name, default_value);
  };
  options_.bitrate = declare("bitrate", int64_t{2'000'000});
  options_.fps = static_cast<int>(declare("fps", int64_t{30}));
  options_.gop = static_cast<int>(declare("gop", int64_t{options_.fps}));
  options_.encoder = declare("encoder", std::string());
}

void H264Publisher::publish(
  const sensor_msgs::msg::Image & image, const PublishFn & publish_fn) const
{
  std::lock_guard<std::mutex> lock(mutex_);

  // image_transport calls publish() only while the topic has subscribers.
  const size_t subscribers = getNumSubscribers();
  const auto now = std::chrono::steady_clock::now();
  const bool new_subscriber = subscribers > subscribers_ ||
    now - last_image_ > std::chrono::duration<double>(kGapSeconds);
  subscribers_ = subscribers;
  last_image_ = now;

  const bool same_format = encoder_ &&
    encoder_->options().width == static_cast<int>(image.width) &&
    encoder_->options().height == static_cast<int>(image.height) &&
    encoder_->options().encoding == image.encoding;
  if (!same_format) {
    encoder_.reset();
    pending_.clear();
    EncoderOptions options = options_;
    options.width = static_cast<int>(image.width);
    options.height = static_cast<int>(image.height);
    options.encoding = image.encoding;
    const std::string format =
      std::to_string(image.width) + "x" + std::to_string(image.height) + " " + image.encoding;
    try {
      encoder_ = std::make_unique<H264Encoder>(options);
      RCLCPP_INFO(
        logger_, "%s with %s at %ld bit/s", format.c_str(), encoder_->name().c_str(),
        static_cast<long>(options.bitrate));  // NOLINT(runtime/int)
      failed_format_.clear();
    } catch (const std::exception & e) {
      if (format != failed_format_) {
        RCLCPP_ERROR(logger_, "cannot encode %s: %s", format.c_str(), e.what());
        failed_format_ = format;
      }
      return;
    }
  } else if (new_subscriber) {
    encoder_->requestKeyframe();
  }

  if (image.data.size() < static_cast<size_t>(image.step) * image.height) {
    RCLCPP_ERROR(logger_, "image data is smaller than step * height, dropping it");
    return;
  }

  std::vector<EncodedFrame> frames;
  pending_.emplace_back(next_pts_++, image.header);
  try {
    frames = encoder_->encode(image.data.data(), static_cast<int>(image.step));
  } catch (const std::exception & e) {
    RCLCPP_ERROR(logger_, "%s, restarting the encoder", e.what());
    encoder_.reset();
    return;
  }

  for (auto & frame : frames) {
    // Encoders may drop frames; skip the headers of the ones that never came out.
    while (!pending_.empty() && pending_.front().first < frame.pts) {
      pending_.pop_front();
    }
    if (pending_.empty() || pending_.front().first != frame.pts) {
      continue;
    }
    sensor_msgs::msg::CompressedImage msg;
    msg.header = pending_.front().second;
    pending_.pop_front();
    msg.format = "h264";
    msg.data = std::move(frame.data);
    publish_fn(msg);
  }
}

}  // namespace as2_video
