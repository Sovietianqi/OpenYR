cmake_minimum_required(VERSION 3.16)
project(gamerecon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# x86 target (original game is 32-bit)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
endif()

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    add_definitions(-DPLATFORM_WINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
    add_definitions(-DPLATFORM_MACOS)
else()
    set(PLATFORM_LINUX TRUE)
    add_definitions(-DPLATFORM_LINUX)
endif()

# Compiler settings
if(MSVC)
    add_compile_options(/W3 /Zc:__cplusplus /permissive- /utf-8)
    add_compile_options(/wd4068)  # unknown pragma
    add_compile_options(/wd4244)  # conversion warning
    add_compile_options(/wd4305)  # truncation warning
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
    add_compile_options(-fno-rtti -fno-exceptions)
endif()

# Source files
file(GLOB_RECURSE GAMERECON_SOURCES
    "src/*.cpp"
    "src/*.h"
)

add_executable(gamerecon ${GAMERECON_SOURCES})

target_include_directories(gamerecon PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(gamerecon PRIVATE
        winmm ws2_32 ddraw dxguid dsound
        user32 gdi32 kernel32 advapi32 shell32 comctl32
    )
elseif(PLATFORM_LINUX)
    target_link_libraries(gamerecon PRIVATE
        pthread dl
    )
endif()

# Output directory
set_target_properties(gamerecon PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)