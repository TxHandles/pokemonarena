cmake_minimum_required(VERSION 3.16)
project(ArenaClash C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib is downloaded and built automatically the first time you configure.
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)
# Let creature sprites be JPG/BMP as well as PNG.
target_compile_definitions(raylib PRIVATE SUPPORT_FILEFORMAT_JPG SUPPORT_FILEFORMAT_BMP)

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
# (src/autotest.inc is only used by development builds with -DARENA_AUTOTEST)
# ENet (src/enet, MIT license) carries online co-op traffic.
file(GLOB ENET_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/enet/*.c)
add_executable(ArenaClash ${GAME_SOURCES} ${ENET_SOURCES})
target_include_directories(ArenaClash PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/enet/include)
target_link_libraries(ArenaClash PRIVATE raylib)
# The game's data files go in the exe as one blob (python tools/embed_files.py makes it): GCC / Clang link it
# with the assembler's .incbin, Visual Studio as a Windows resource. Rebuild when the blob changes.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_blob.bin)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_blob.cpp PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_blob.bin)
    if(MSVC)
        target_sources(ArenaClash PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_blob.rc)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_blob.rc PROPERTIES
            OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_blob.bin)
    endif()
endif()
if(WIN32)
    target_link_libraries(ArenaClash PRIVATE ws2_32 winmm)
endif()

# The game looks for a Pokemon/ folder next to the exe.
add_custom_command(TARGET ArenaClash POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Pokemon $<TARGET_FILE_DIR:ArenaClash>/Pokemon
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/story.txt $<TARGET_FILE_DIR:ArenaClash>/story.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/map.png $<TARGET_FILE_DIR:ArenaClash>/map.png)

if(MSVC)
    target_compile_options(ArenaClash PRIVATE /W3 /utf-8)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ArenaClash)
    # Release builds open without a console window.
    set_target_properties(ArenaClash PROPERTIES
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
else()
    target_compile_options(ArenaClash PRIVATE -Wall -Wextra)
endif()
