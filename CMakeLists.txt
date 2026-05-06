cmake_minimum_required(VERSION 3.25)

project(Smasttrafik
    VERSION 0.1.0
    DESCRIPTION "Smasttrafik delay analytics for Vasttrafik bus traffic"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SMASTTRAFIK_ENABLE_POSTGRES "Build the libpqxx-backed PostgreSQL repository" OFF)
option(SMASTTRAFIK_BUILD_TESTS "Build backend unit tests" ON)

include(FetchContent)

FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.3.2
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(Crow nlohmann_json)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

set(SMASTTRAFIK_CORE_SOURCES
    backend/src/config.cpp
    backend/src/http_client.cpp
    backend/src/time_utils.cpp
    backend/src/token_bucket.cpp
    backend/src/vasttrafik_client.cpp
    backend/src/collectors/traffic_collector.cpp
    backend/src/repositories/demo_repository.cpp
    backend/src/routes/api_routes.cpp
    backend/src/stats_service.cpp
)

if(SMASTTRAFIK_ENABLE_POSTGRES)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
    list(APPEND SMASTTRAFIK_CORE_SOURCES backend/src/repositories/postgres_repository.cpp)
endif()

add_library(smasttrafik_core ${SMASTTRAFIK_CORE_SOURCES})
target_include_directories(smasttrafik_core PUBLIC backend/include)
target_link_libraries(smasttrafik_core
    PUBLIC
        Crow::Crow
        nlohmann_json::nlohmann_json
        CURL::libcurl
        Threads::Threads
)

if(SMASTTRAFIK_ENABLE_POSTGRES)
    target_compile_definitions(smasttrafik_core PUBLIC SMASTTRAFIK_ENABLE_POSTGRES=1)
    target_link_libraries(smasttrafik_core PUBLIC PkgConfig::PQXX)
endif()

if(MSVC)
    target_compile_options(smasttrafik_core PRIVATE /W4)
else()
    target_compile_options(smasttrafik_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(smasttrafik-api backend/src/main.cpp)
target_link_libraries(smasttrafik-api PRIVATE smasttrafik_core)

add_executable(smasttrafik-worker backend/src/worker_main.cpp)
target_link_libraries(smasttrafik-worker PRIVATE smasttrafik_core)

if(SMASTTRAFIK_BUILD_TESTS)
    enable_testing()
    add_executable(smasttrafik-tests backend/tests/time_utils_tests.cpp)
    target_link_libraries(smasttrafik-tests PRIVATE smasttrafik_core)
    add_test(NAME smasttrafik-tests COMMAND smasttrafik-tests)
endif()
