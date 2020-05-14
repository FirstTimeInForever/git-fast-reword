cmake_minimum_required(VERSION 3.10)
project(git-fast-reword)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX") # CMake already contains W3 in its flags
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
endif()


message(${CMAKE_CXX_FLAGS_RELEASE})

add_subdirectory(libs/libgit2)
include_directories(libs/libgit2/include)
link_directories(libs/libgit2)

add_executable(git-fast-reword src/main.cpp)
target_link_libraries(git-fast-reword git2)
