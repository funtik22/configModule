cmake_minimum_required(VERSION 3.16)
project(MPlaneConfigModule LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SYSREPO_CPP REQUIRED sysrepo-cpp)
pkg_check_modules(LIBYANG_CPP REQUIRED libyang-cpp)

add_executable(mplane_config_module
    src/main.cpp
    src/ConfigModule.cpp
    src/ConfigTypes.cpp
    src/ConfigValidator.cpp

    libs/messageBus/SharedMemory/src/BaseMemory.cpp
)

target_include_directories(mplane_config_module
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/messageBus/SharedMemory/include/
        ${SYSREPO_CPP_INCLUDE_DIRS}
        ${LIBYANG_CPP_INCLUDE_DIRS}
)

target_link_libraries(mplane_config_module
    PRIVATE
        ${SYSREPO_CPP_LIBRARIES}
        ${LIBYANG_CPP_LIBRARIES}
)

target_compile_options(mplane_config_module PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)