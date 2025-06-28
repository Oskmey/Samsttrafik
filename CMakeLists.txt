cmake_minimum_required(VERSION 3.10)
project(SamsttrafikBackend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} backend/include)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} backend/src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads ws2_32)


