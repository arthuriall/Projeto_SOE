cmake_minimum_required(VERSION 3.10)
project(Projeto_SOE)

set(CMAKE_CXX_STANDARD 14)

# Busca o OpenCV instalado
find_package(OpenCV REQUIRED)

# Adiciona os arquivos-fonte do projeto
add_executable(projeto
    main.cpp
    camera.cpp
    lcd.cpp
)

# Inclui diretórios de cabeçalhos
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    .
)

# Linka com bibliotecas do OpenCV e WiringPi
target_link_libraries(projeto
    ${OpenCV_LIBS}
    wiringPi
)
