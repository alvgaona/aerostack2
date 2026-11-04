cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME as2_video)
project(${PROJECT_NAME})

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# set Release as default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# find dependencies
set(PROJECT_DEPENDENCIES
  image_transport
  pluginlib
  rclcpp
  sensor_msgs
)

find_package(ament_cmake REQUIRED)
foreach(DEPENDENCY ${PROJECT_DEPENDENCIES})
  find_package(${DEPENDENCY} REQUIRED)
endforeach()
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavutil libswscale)

# H.264 encoder and decoder, free of ROS
add_library(${PROJECT_NAME} SHARED
  src/h264_encoder.cpp
  src/h264_decoder.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::FFMPEG)

# image_transport "h264" plugins
add_library(${PROJECT_NAME}_image_transport SHARED
  src/h264_publisher.cpp
  src/h264_subscriber.cpp
  src/manifest.cpp
)
target_link_libraries(${PROJECT_NAME}_image_transport
  ${PROJECT_NAME}
  image_transport::image_transport
  pluginlib::pluginlib
  rclcpp::rclcpp
  ${sensor_msgs_TARGETS}
)
# image_transport 4 (Iron and later) passes publisher and subscription options to the plugins.
if(image_transport_VERSION VERSION_GREATER_EQUAL 4.0)
  target_compile_definitions(${PROJECT_NAME}_image_transport PRIVATE AS2_VIDEO_IMAGE_TRANSPORT_OPTIONS)
endif()
pluginlib_export_plugin_description_file(image_transport h264_plugins.xml)

install(
  DIRECTORY include/
  DESTINATION include
)

install(
  TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_image_transport
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(${PROJECT_DEPENDENCIES})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
  add_subdirectory(tests)
endif()

ament_package()
