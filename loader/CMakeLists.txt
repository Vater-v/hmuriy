cmake_minimum_required(VERSION 3.10.2)

project("ValeraLoader")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(valera_loader SHARED main.cpp)

find_library(
        log-lib
        log
)

target_link_libraries(
        valera_loader
        ${log-lib}
)