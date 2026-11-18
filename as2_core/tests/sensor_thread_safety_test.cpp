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

// Thread safety of sensors with a publication rate.
//
// Such a sensor publishes from its own timer, while the platform calls updateData()
// from another thread (another callback group on a MultiThreadedExecutor). Without a
// lock, the timer can publish while a message is half written.
//
// Every test follows the same recipe:
//   1. Build two messages, A and B, each consistent on its own: every field derived
//      from the same value (1 for A, 2 for B).
//   2. Call race(): the timer publishes at 1 kHz for one second while a writer thread
//      keeps calling updateData() with A, B, A, B, ...
//   3. Check every published message. One that mixes fields from A and B was read in
//      the middle of an update, so it counts as torn.
// A test passes only when no message is torn.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "as2_core/sensor.hpp"
#include "gtest/gtest.h"

namespace as2
{
namespace sensors
{

using namespace std::chrono_literals;

// High enough to get ~1000 publications in the one-second race.
constexpr float kPubFreq = 1000.0f;

// Spins `nodes` (the sensor timer and any subscriber) on this thread for one second,
// while a writer thread calls `write(i)` with i = 0, 1, 2, ...
void race(
  const std::vector<rclcpp::Node::SharedPtr> & nodes,
  const std::function<void(int)> & write)
{
  rclcpp::executors::SingleThreadedExecutor executor;
  for (const auto & node : nodes) {
    executor.add_node(node);
  }
  std::atomic<bool> stop{false};
  std::thread writer([&] {
      for (int i = 0; !stop; ++i) {
        write(i);
        // An unfair mutex taken in a tight loop would starve the timer.
        std::this_thread::sleep_for(50us);
      }
    });
  const auto end = std::chrono::steady_clock::now() + 1s;
  while (std::chrono::steady_clock::now() < end) {
    executor.spin_some(10ms);
  }
  stop = true;
  writer.join();
}

// Published and torn message counts, filled by a subscription or a probe.
struct Counts
{
  std::atomic<int> published{0};
  std::atomic<int> torn{0};

  void record(bool whole)
  {
    published++;
    if (!whole) {
      torn++;
    }
  }
};

// Subscribes to `topic` and records whether each received message is whole.
template<typename MsgT>
typename rclcpp::Subscription<MsgT>::SharedPtr checkEveryMessage(
  const rclcpp::Node::SharedPtr & node, const std::string & topic,
  bool (* is_whole)(const MsgT &), Counts & counts)
{
  return node->create_subscription<MsgT>(
    topic, rclcpp::SensorDataQoS(),
    [is_whole, &counts](const MsgT & msg) {counts.record(is_whole(msg));});
}

void expectNoneTorn(const Counts & counts)
{
  // Guards against a vacuous pass: the race must have published something.
  ASSERT_GT(counts.published, 10);
  EXPECT_EQ(counts.torn, 0) <<
    counts.torn << " of " << counts.published << " publications mixed two updates";
}

// Sensor<T>, the template behind Imu, GPS, Lidar and the other simple sensors.

// The ranges are large so the timer can land in the middle of copying them.
sensor_msgs::msg::LaserScan makeScan(int value)
{
  sensor_msgs::msg::LaserScan scan;
  scan.header.stamp.sec = value;
  scan.ranges.assign(100000, static_cast<float>(value));
  return scan;
}

bool scanIsWhole(const sensor_msgs::msg::LaserScan & scan)
{
  const auto value = static_cast<float>(scan.header.stamp.sec);
  return std::all_of(
    scan.ranges.begin(), scan.ranges.end(), [value](float r) {return r == value;});
}

TEST(SensorThreadSafetyTest, SensorPublishesWholeMessage) {
  auto node = std::make_shared<as2::Node>("sensor_race_node");
  auto sub_node = std::make_shared<rclcpp::Node>("sensor_race_sub");
  Sensor<sensor_msgs::msg::LaserScan> sensor("race_scan", node.get(), kPubFreq);
  const std::array<sensor_msgs::msg::LaserScan, 2> scans{makeScan(1), makeScan(2)};
  sensor.updateData(scans[0]);

  Counts counts;
  auto sub = checkEveryMessage(sub_node, sensor.getTopicName(), &scanIsWhole, counts);
  race({node, sub_node}, [&](int i) {sensor.updateData(scans[i % 2]);});

  expectNoneTorn(counts);
}

// Camera keeps its own image instead of using SensorData, so it needs its own test.

sensor_msgs::msg::Image makeImage(int value)
{
  sensor_msgs::msg::Image image;
  image.header.stamp.sec = value;
  image.height = 480;
  image.width = 640;
  image.encoding = "rgb8";
  image.step = 640 * 3;
  image.data.assign(480 * 640 * 3, static_cast<uint8_t>(value));
  return image;
}

bool imageIsWhole(const sensor_msgs::msg::Image & image)
{
  const auto value = static_cast<uint8_t>(image.header.stamp.sec);
  return std::all_of(
    image.data.begin(), image.data.end(), [value](uint8_t b) {return b == value;});
}

TEST(SensorThreadSafetyTest, CameraPublishesWholeImage) {
  auto node = std::make_shared<as2::Node>("camera_race_node");
  auto sub_node = std::make_shared<rclcpp::Node>("camera_race_sub");
  Camera camera(node.get(), "race_cam", kPubFreq);
  const std::array<sensor_msgs::msg::Image, 2> images{makeImage(1), makeImage(2)};
  // The publisher is created on the first update, before the timer may publish.
  camera.updateData(images[0]);

  Counts counts;
  const auto topic = SensorData<sensor_msgs::msg::Image>::processTopicName("race_cam/image");
  auto sub = checkEveryMessage(sub_node, topic, &imageIsWhole, counts);
  race({node, sub_node}, [&](int i) {camera.updateData(images[i % 2]);});

  expectNoneTorn(counts);
}

// Gimbal and GroundTruth publish two messages per tick on separate topics, and the
// bug there is a mismatched pair rather than a single torn message. Matching messages
// across two subscriptions is unreliable, so a probe subclass checks the pair inside
// publishData(), right where the timer publishes it.
//
// Their messages are tiny, so an update finishes too fast for the timer to land inside
// it. Copying this long frame_id makes each update slow enough to be caught.
const std::string kLongFrameId(100000, 'f');

class GimbalProbe : public Gimbal
{
public:
  using Gimbal::Gimbal;

  Counts counts;

protected:
  void publishData() override
  {
    Gimbal::publishData();
    counts.record(getData().pose.position.x == gimbal_transform_.transform.translation.x);
  }
};

geometry_msgs::msg::PoseStamped makeGimbalPose(int value)
{
  geometry_msgs::msg::PoseStamped pose;
  pose.pose.position.x = value;
  return pose;
}

TEST(SensorThreadSafetyTest, GimbalPublishesMatchingPoseAndTransform) {
  auto node = std::make_shared<as2::Node>("gimbal_race_node");
  // updateData() copies the base frame id after storing the pose and before writing the
  // transform translation. A long base id widens exactly that gap.
  GimbalProbe gimbal("race_gimbal", kLongFrameId, node.get(), kPubFreq);
  const std::array<geometry_msgs::msg::PoseStamped, 2> poses{
    makeGimbalPose(1), makeGimbalPose(2)};
  gimbal.updateData(poses[0]);

  race({node}, [&](int i) {gimbal.updateData(poses[i % 2]);});

  expectNoneTorn(gimbal.counts);
}

// updateData(pose, twist) stores the pose, then the twist. A publication between the two
// would send a new pose with an old twist.
class GroundTruthProbe : public GroundTruth
{
public:
  using GroundTruth::GroundTruth;

  Counts counts;

protected:
  void publishData() override
  {
    GroundTruth::publishData();
    counts.record(
      pose_sensor_->getData().header.stamp.sec == twist_sensor_->getData().header.stamp.sec);
  }
};

geometry_msgs::msg::PoseStamped makeGroundTruthPose(int value)
{
  geometry_msgs::msg::PoseStamped pose;
  pose.header.stamp.sec = value;
  pose.header.frame_id = kLongFrameId;
  return pose;
}

geometry_msgs::msg::TwistStamped makeGroundTruthTwist(int value)
{
  geometry_msgs::msg::TwistStamped twist;
  twist.header.stamp.sec = value;
  twist.header.frame_id = kLongFrameId;
  return twist;
}

TEST(SensorThreadSafetyTest, GroundTruthPublishesMatchingPair) {
  auto node = std::make_shared<as2::Node>("ground_truth_race_node");
  GroundTruthProbe ground_truth(node.get(), kPubFreq);
  const std::array<geometry_msgs::msg::PoseStamped, 2> poses{
    makeGroundTruthPose(1), makeGroundTruthPose(2)};
  const std::array<geometry_msgs::msg::TwistStamped, 2> twists{
    makeGroundTruthTwist(1), makeGroundTruthTwist(2)};
  ground_truth.updateData(poses[0], twists[0]);

  race({node}, [&](int i) {ground_truth.updateData(poses[i % 2], twists[i % 2]);});

  expectNoneTorn(ground_truth.counts);
}

}  // namespace sensors
}  // namespace as2

int main(int argc, char * argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  auto result = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return result;
}
