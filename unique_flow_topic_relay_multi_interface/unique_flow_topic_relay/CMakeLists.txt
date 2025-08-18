cmake_minimum_required(VERSION 3.8)
project(unique_flow_topic_relay)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(common_interfaces REQUIRED)

find_package(fastrtps_cmake_module REQUIRED)
find_package(fastrtps 2 REQUIRED CONFIG)
find_package(FastRTPS 2 REQUIRED MODULE)

find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(unique_topic_flow_interface
  "msg/Serialized.msg"
  "msg/MOS.msg"
  "msg/Locator.msg"
  "msg/Throttle.msg"
  "msg/TunnelSelect.msg"
  "msg/CoeffForTopic.msg"
  "msg/UniqueTopicFlow.msg"
  "msg/UniqueTopicFlowStat.msg"
  "msg/UniqueTopicFlowTunnelStat.msg"
  "msg/UniqueTopicFlowStats.msg"
  DEPENDENCIES builtin_interfaces
  LIBRARY_NAME ${PROJECT_NAME}
)

ament_export_dependencies(rosidl_default_runtime)

add_library(subscriber_discovery src/subscriber_discovery.cpp)
target_link_libraries(subscriber_discovery fastcdr fastrtps)

add_executable(unique_flow_topic_relay src/relay.cpp)
add_executable(unique_flow_topic_relay_endpoint src/relay_endpoint.cpp)

ament_target_dependencies(unique_flow_topic_relay rclcpp)
ament_target_dependencies(unique_flow_topic_relay_endpoint rclcpp)
rosidl_get_typesupport_target(cpp_typesupport_target unique_topic_flow_interface "rosidl_typesupport_cpp")
target_link_libraries(unique_flow_topic_relay subscriber_discovery "${cpp_typesupport_target}")
target_link_libraries(unique_flow_topic_relay_endpoint "${cpp_typesupport_target}")

# Install nodes
install(
  TARGETS unique_flow_topic_relay unique_flow_topic_relay_endpoint subscriber_discovery
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)



if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # uncomment the line when a copyright and license is not present in all source files
  #set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # uncomment the line when this package is not in a git repo
  #set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
