cmake_minimum_required(VERSION 3.14)
project(ucx_rma_demo C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Configure UCX path from environment instead of pkg-config
set(UCX_INSTALL_PATH $ENV{INSTALL_UCX_PATH})
if (NOT UCX_INSTALL_PATH)
    message(FATAL_ERROR "INSTALL_UCX_PATH is not set")
else()
    message(STATUS "UCX_INSTALL_PATH: ${UCX_INSTALL_PATH}")
    include_directories(${UCX_INSTALL_PATH}/include)
    link_directories(${UCX_INSTALL_PATH}/lib)
    set(UCX_LIBRARY_OBJ ucp ucs uct ucm)
endif()

add_executable(ucx_rma_server server.cpp ucx_util.cpp)
add_executable(ucx_rma_client client.cpp ucx_util.cpp)

target_link_libraries(ucx_rma_server PRIVATE ${UCX_LIBRARY_OBJ})
target_link_libraries(ucx_rma_client PRIVATE ${UCX_LIBRARY_OBJ})

# Helpful: bake rpath to UCX libs for run-from-build-tree convenience
if (APPLE)
    set(CMAKE_BUILD_RPATH "${UCX_INSTALL_PATH}/lib")
else()
    set(CMAKE_BUILD_RPATH "${UCX_INSTALL_PATH}/lib")
endif()
