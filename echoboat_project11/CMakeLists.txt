cmake_minimum_required(VERSION 3.5)
project(echoboat_project11)

find_package(ament_cmake REQUIRED)
find_package(marine_autonomy REQUIRED)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(test_param_compose test/test_param_compose.py)
endif()

ament_package()
