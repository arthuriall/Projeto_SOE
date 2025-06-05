cmake_minimum_required(VERSION 3.10)
project(Projeto_SOE)

set(CMAKE_CXX_STANDARD 14)

find_package(OpenCV REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    include
)

add_executable(projeto
    src/main.cpp
    src/camera.cpp
    src/lcd.cpp
)

target_link_libraries(projeto
    ${OpenCV_LIBS}
    wiringPi
)
