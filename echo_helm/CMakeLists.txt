cmake_minimum_required(VERSION 3.5.1)

project(echo_helm)

set(CMAKE_CXX_STANDARD 17)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

find_package(diagnostic_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(mavros REQUIRED)
find_package(mavros_msgs REQUIRED)
find_package(marine_interfaces REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(${PROJECT_NAME}_node src/echo_helm_node.cpp)

ament_target_dependencies(${PROJECT_NAME}_node geometry_msgs mavros mavros_msgs marine_interfaces rclcpp rclcpp_lifecycle)

# Removes the geoid round trip from mavros's NavSatFix altitudes; see
# include/echo_helm/ellipsoidal_corrector.hpp.
add_executable(ellipsoidal_fix_node src/ellipsoidal_fix_node.cpp)
# PRIVATE, not PUBLIC: the header is an implementation detail of this package's
# executables and its unit test. Nothing links against an executable target, so
# an INSTALL_INTERFACE on one is inert, and the package exports no include
# directories.
target_include_directories(ellipsoidal_fix_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(ellipsoidal_fix_node
  diagnostic_msgs mavros_msgs rclcpp sensor_msgs)

install(TARGETS ${PROJECT_NAME}_node ellipsoidal_fix_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_ellipsoidal_corrector test/test_ellipsoidal_corrector.cpp)
  target_include_directories(test_ellipsoidal_corrector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

ament_package()
