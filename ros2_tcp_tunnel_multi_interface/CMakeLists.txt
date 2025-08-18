cmake_minimum_required(VERSION 3.8)
project(tcp_tunnel)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_BUILD_TYPE "Release")

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "srv/RegisterClient.srv"
  "srv/AddTopic.srv"
  "srv/RemoveTopic.srv"
  DEPENDENCIES std_msgs
)

include_directories(include)

add_executable(server src/server.cpp src/semaphore.cpp)
ament_target_dependencies(server
  rclcpp
  std_msgs
)

add_executable(client src/client.cpp src/semaphore.cpp)
ament_target_dependencies(client
  rclcpp
  std_msgs
)

add_executable(ros2_tcp_tunnel_service_server src/ros2_tcp_tunnel_service_server.cpp src/semaphore.cpp)
ament_target_dependencies(ros2_tcp_tunnel_service_server
  rclcpp
  std_msgs
)

add_executable(ros2_tcp_tunnel_service_client src/ros2_tcp_tunnel_service_client.cpp src/semaphore.cpp)
ament_target_dependencies(ros2_tcp_tunnel_service_client
  rclcpp
  std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(client "${cpp_typesupport_target}")
target_link_libraries(server "${cpp_typesupport_target}")
target_link_libraries(ros2_tcp_tunnel_service_client "${cpp_typesupport_target}")
target_link_libraries(ros2_tcp_tunnel_service_server "${cpp_typesupport_target}")
ament_export_dependencies(rosidl_default_runtime)

install(TARGETS client server ros2_tcp_tunnel_service_client ros2_tcp_tunnel_service_server
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
