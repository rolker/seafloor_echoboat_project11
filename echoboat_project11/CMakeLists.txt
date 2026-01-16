cmake_minimum_required(VERSION 3.5)
project(echoboat_project11)

find_package(marine_autonomy REQUIRED)


install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})

ament_package()
