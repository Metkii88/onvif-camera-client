cmake_minimum_required(VERSION 3.16)
project(onvif-camera-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# --- Qt5 (Widgets + Network для WS-Discovery) ---
find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

# --- FFmpeg / libav (в Astra Linux SE 1.8 это libav из пакета libav-dev) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(AV REQUIRED
    libavformat
    libavcodec
    libavutil
    libswscale)

add_executable(onvif-client
    src/main.cpp
    src/mainwindow.cpp
    src/onvifdiscovery.cpp
    src/cameradecoder.cpp
    src/videocell.cpp
)

target_include_directories(onvif-client PRIVATE ${AV_INCLUDE_DIRS})
target_link_directories(onvif-client PRIVATE ${AV_LIBRARY_DIRS})
target_link_libraries(onvif-client PRIVATE
    Qt5::Widgets
    Qt5::Network
    ${AV_LIBRARIES})
target_compile_options(onvif-client PRIVATE ${AV_CFLAGS_OTHER})

install(TARGETS onvif-client RUNTIME DESTINATION bin)
