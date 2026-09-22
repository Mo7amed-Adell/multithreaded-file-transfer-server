add_library(thread_pool STATIC ThreadPool.cpp)

target_include_directories(thread_pool
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Threads REQUIRED)

target_link_libraries(thread_pool
    PUBLIC
        Threads::Threads
)