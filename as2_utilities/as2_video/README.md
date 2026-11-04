# as2_video

H.264 video for AeroStack2 cameras. It adds an `h264` [image_transport](https://github.com/ros-perception/image_common)
plugin, so every camera that publishes through `as2::sensors::Camera` (USB camera, RealSense, Gazebo and the
external platforms) also streams `sensor_measurements/<camera>/image/h264` without code changes. A 640x480 camera at 30 Hz drops
from 27.6 MB/s raw to 240 kB/s at the default bitrate (measured on macOS with VideoToolbox), which fits over Wi-Fi to a
ground station.

- Hardware encoding when there is any: VideoToolbox on macOS, NVENC or V4L2 (Jetson, Raspberry Pi) on Linux.
  Falls back to libx264 or OpenH264.
- The encoder only runs while someone subscribes, and every new subscriber gets a keyframe right away.
- No B-frames, and SPS/PPS repeat on every keyframe, so a subscriber can join or reconnect at any time.
- Each message keeps the header (stamp and frame) of the image it came from.

## Usage

Once the package is installed, image_transport picks it up. List the topics of a drone's camera:

```bash
ros2 topic list | grep h264
# /drone0/sensor_measurements/camera/image/h264
```

Decode it back to raw images on the ground station, for example to feed RViz or a detector:

```bash
# Jazzy
ros2 run image_transport republish --ros-args -p in_transport:=h264 -p out_transport:=raw \
  -r in/h264:=/drone0/sensor_measurements/camera/image/h264 -r out:=/drone0/camera_decoded
# Humble
ros2 run image_transport republish h264 raw --ros-args \
  -r in/h264:=/drone0/sensor_measurements/camera/image/h264 -r out:=/drone0/camera_decoded
```

`republish` only decodes while its output has subscribers.

Any image_transport subscriber can ask for the transport directly, with the `image_transport` parameter set to
`h264` or in code:

```cpp
auto sub = image_transport::create_subscription(
  node.get(), "sensor_measurements/camera/image", callback, "h264");
```

Cameras that do not use image_transport can be re-published as H.264. The transport topic needs its own remap:

```bash
# Jazzy
ros2 run image_transport republish --ros-args -p in_transport:=raw -p out_transport:=h264 \
  -r in:=/camera/image_raw -r out/h264:=/camera/video/h264
# Humble
ros2 run image_transport republish raw h264 --ros-args \
  -r in:=/camera/image_raw -r out/h264:=/camera/video/h264
```

Re-publishing sends raw frames between processes first. On Humble, the default Fast DDS can fall behind with large
frames (6 Hz instead of 30 Hz for 640x480 in our tests); Cyclone DDS (`RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`) or
running in the camera's process avoids it.

## Parameters

Declared on the node that owns the camera, under the camera topic with the node namespace removed and `/` replaced
by `.`. For `/drone0/sensor_measurements/camera/image` they are
`sensor_measurements.camera.image.h264.<name>`:

| Name      | Default   | Description                                                                    |
| --------- | --------- | ------------------------------------------------------------------------------ |
| `bitrate` | `2000000` | Target bitrate in bits/s.                                                      |
| `fps`     | `30`      | Expected frame rate, used for rate control.                                   |
| `gop`     | = `fps`   | Frames between keyframes. Late subscribers already get one when they join.     |
| `encoder` | `""`      | FFmpeg encoder name, such as `h264_nvenc` or `libx264`. Empty picks the first that opens. |

They are read when the camera starts.

## Message format

`sensor_msgs/CompressedImage` with `format: "h264"` and one H.264 Annex B access unit in `data`. Accepted input
encodings are `bgr8`, `rgb8`, `bgra8`, `rgba8`, `mono8`, `yuv422` and `yuv422_yuy2`, with even width and height.
The subscriber outputs `bgr8`.

## Library

The encoder and decoder do not depend on ROS and can be used directly:

```cpp
#include <as2_video/h264_encoder.hpp>

as2_video::EncoderOptions options;
options.width = 640;
options.height = 480;
options.encoding = "rgb8";
as2_video::H264Encoder encoder(options);

for (const auto & frame : encoder.encode(pixels, stride)) {
  send(frame.data);  // Annex B access unit; frame.pts is the index of the input image
}
```

Link it with `find_package(as2_video)` and `as2_video::as2_video`.
