cmake_minimum_required(VERSION 3.10)

project(WebcamSimple)

set(CMAKE_CXX_STANDARD 11)

find_package(OpenCV REQUIRED)

# Corrigido para 'include' (sem 's')
include_directories(${OpenCV_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include)

add_executable(webcam
    src/main.cpp
    src/camera.cpp
)

target_link_libraries(webcam ${OpenCV_LIBS})
