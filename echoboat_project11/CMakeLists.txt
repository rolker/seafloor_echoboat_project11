cmake_minimum_required(VERSION 3.5)
project(echoboat_project11)

find_package(ament_cmake REQUIRED)
# marine_autonomy is a runtime (launch-include) dependency only — declared as
# exec_depend in package.xml; no find_package needed for this config-only build.
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(test_param_compose test/test_param_compose.py)
  ament_add_pytest_test(test_launch_wiring test/test_launch_wiring.py)
endif()

ament_package()
