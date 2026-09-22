cmake_minimum_required(VERSION 3.16)
project(file_transfer_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(thread_pool)
add_subdirectory(server)
add_subdirectory(client_cli)