cmake_minimum_required(VERSION 3.8)
project(chacras_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/MotorState.msg"
  "msg/MotorStateArray.msg"
  "msg/SystemStatus.msg"
  "msg/MotorCmd.msg"
)

ament_package()