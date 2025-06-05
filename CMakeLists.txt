cmake_minimum_required(VERSION 3.10)
project(ProjetoLaserBotao)

set(CMAKE_CXX_STANDARD 17)

# OpenCV
find_package(OpenCV REQUIRED)

# Arquivos-fonte
file(GLOB SOURCES "src/*.cpp")

add_executable(projeto ${SOURCES})

# Incluir diretórios com os headers
target_include_directories(projeto PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
)

# Link com bibliotecas
target_link_libraries(projeto
    ${OpenCV_LIBS}
    wiringPi
)
