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
* @file h264_transport_gtest.cpp
*
* Publishes images through image_transport and receives them over the h264 transport.
*
* @author Alvaro J. Gaona
*/

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <vector>

#include <image_transport/image_transport.hpp>
#include <image_transport/transport_hints.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>

TEST(H264Transport, ImagesArriveDecoded)
{
  auto node = std::make_shared<rclcpp::Node>("h264_transport_test", "drone0");
  image_transport::ImageTransport it(node);
  auto publisher = it.advertise("camera/image_raw", 10);

  EXPECT_TRUE(node->has_parameter("camera.image_raw.h264.bitrate"));
  EXPECT_TRUE(node->has_parameter("camera.image_raw.h264.encoder"));

  std::vector<sensor_msgs::msg::Image::ConstSharedPtr> received;
  const image_transport::TransportHints hints(node.get(), "h264");
  auto subscriber = it.subscribe(
    "camera/image_raw", 10,
    [&received](const sensor_msgs::msg::Image::ConstSharedPtr & image) {
      received.push_back(image);
    }, nullptr, &hints);
  EXPECT_EQ(subscriber.getTransport(), "h264");
  EXPECT_EQ(subscriber.getTopic(), "/drone0/camera/image_raw/h264");

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);

  sensor_msgs::msg::Image image;
  image.width = 160;
  image.height = 120;
  image.encoding = "rgb8";
  image.step = image.width * 3;
  image.header.frame_id = "camera_optical";

  std::set<int32_t> sent;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  for (int32_t i = 0; received.size() < 10 && std::chrono::steady_clock::now() < deadline; ++i) {
    image.data.assign(image.step * image.height, static_cast<uint8_t>(i * 5));
    image.header.stamp.sec = i;
    publisher.publish(image);
    sent.insert(i);
    executor.spin_some(std::chrono::milliseconds(30));
  }

  ASSERT_GE(received.size(), 10u);
  for (const auto & out : received) {
    EXPECT_EQ(out->width, 160u);
    EXPECT_EQ(out->height, 120u);
    EXPECT_EQ(out->encoding, "bgr8");
    EXPECT_EQ(out->step, 160u * 3);
    EXPECT_EQ(out->header.frame_id, "camera_optical");
    EXPECT_TRUE(sent.count(out->header.stamp.sec));
    // Every pixel was set to stamp * 5; H.264 keeps flat gray within a few levels.
    EXPECT_NEAR(out->data[out->data.size() / 2], (out->header.stamp.sec * 5) % 256, 6);
  }
  // Stamps follow the images they belong to, in order.
  for (size_t i = 1; i < received.size(); ++i) {
    EXPECT_LT(received[i - 1]->header.stamp.sec, received[i]->header.stamp.sec);
  }
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  const int result = RUN_ALL_TESTS();
  rclcpp::shutdown();
#ifdef __APPLE__
  // class_loader aborts in its static destructors on macOS once any image_transport plugin
  // has been loaded, even the stock ones. Skip static teardown so the result is kept.
  std::fflush(nullptr);
  std::_Exit(result);
#endif
  return result;
}
