find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)

add_executable(client_gui main.cpp mainwindow.cpp mainwindow.h)
target_link_libraries(client_gui PRIVATE Qt6::Widgets Qt6::Network)