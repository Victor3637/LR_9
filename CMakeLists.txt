cimake_minimum_required(VERSION 3.10)
project(FileMonitor)

set(CMAKE_CXX_STANDARD 17)

add_executable(server src/server.cpp src/file_utils.cpp src/cache.cpp src/ipc_utils.cpp)
add_executable(client src/client.cpp)
