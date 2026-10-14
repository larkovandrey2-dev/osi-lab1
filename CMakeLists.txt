cmake_minimum_required(VERSION 3.16)
project(lab2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(os_layer STATIC)
target_include_directories(os_layer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/os)

if(WIN32)
    target_sources(os_layer PRIVATE os/os_win.c)
elseif(UNIX)
    target_sources(os_layer PRIVATE os/os_posix.c)
else()
    message(FATAL_ERROR "Unsupported operating system")
endif()

foreach(program parent child1 child2)
    add_executable(${program} src/${program}.c)
    target_link_libraries(${program} PRIVATE os_layer)
endforeach()

add_dependencies(parent child1 child2)

foreach(target os_layer parent child1 child2)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
