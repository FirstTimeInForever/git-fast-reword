cmake_minimum_required(VERSION 3.10)
project(git-fast-reword)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX") # CMake already contains W3 in its flags
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
    # Suppress weird libgit2 documentation warning
    add_compile_options("-Wno-documentation-deprecated-sync")
endif()

set(BUILD_CLAR OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(libs/libgit2)
include_directories(libs/libgit2/include)
link_directories(libs/libgit2)

add_executable(git-fast-reword src/main.cpp)
target_link_libraries(git-fast-reword git2)
