cmake_minimum_required(VERSION 3.16)
project(zenbedded_sine_wave)

find_package(ament_cmake REQUIRED)

install(DIRECTORY urdf config launch DESTINATION share/${PROJECT_NAME})

ament_package()
