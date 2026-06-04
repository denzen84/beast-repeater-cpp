cmake_minimum_required(VERSION 3.25)
project(beast-repeater LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(beast-repeater
    main.cpp
    Application.cpp
    beast/Protocol.cpp
    avr/Protocol.cpp
    codec/Transcoder.cpp
    net/Socket.cpp
    net/Poll.cpp
    core/MessageHub.cpp
    endpoints/InputConnector.cpp
    endpoints/InputServer.cpp
    endpoints/OutputConnector.cpp
    endpoints/OutputServer.cpp
)

target_include_directories(beast-repeater PRIVATE ${CMAKE_SOURCE_DIR})

target_compile_options(beast-repeater PRIVATE
    -Wall -Wextra -Wpedantic -O2
)

target_link_libraries(beast-repeater PRIVATE pthread)
