cmake_minimum_required(VERSION 3.5.1)
project(Brandes)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-pthread -O3")

set(SOURCE_FILES brandes.cc)
add_executable(brandes ${SOURCE_FILES})