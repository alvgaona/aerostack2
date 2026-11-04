find_package(ament_cmake_gtest REQUIRED)

ament_add_gtest(${PROJECT_NAME}_h264_codec_gtest h264_codec_gtest.cpp)
target_link_libraries(${PROJECT_NAME}_h264_codec_gtest ${PROJECT_NAME})

# Loads the plugins through pluginlib, so the package must be installed first.
ament_add_gtest(${PROJECT_NAME}_h264_transport_gtest h264_transport_gtest.cpp
  TIMEOUT 60 SKIP_LINKING_MAIN_LIBRARIES)
target_link_libraries(${PROJECT_NAME}_h264_transport_gtest
  image_transport::image_transport rclcpp::rclcpp ${sensor_msgs_TARGETS})
