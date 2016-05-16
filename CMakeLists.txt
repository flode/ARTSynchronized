cmake_minimum_required(VERSION 2.8)
project(ARTSynchronized)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -march=native -g")

if( NOT CMAKE_BUILD_TYPE )
    message(STATUS "No build type selected, default to Release")
    set( CMAKE_BUILD_TYPE Release)
else()
    message(STATUS "Build type is set to ${CMAKE_BUILD_TYPE}")
endif()


find_library(JemallocLib jemalloc)
find_package (Threads)
set(JemallocLibs ${JemallocLib} ${CMAKE_THREAD_LIBS_INIT})


set(ART_FILES OptimisticLockCoupling/Tree.cpp ROWEX/Tree.cpp)
add_library(ARTSynchronized ${ART_FILES})
target_link_libraries(ARTSynchronized tbb ${JemallocLibs})


set(SOURCE_FILES main.cpp)
add_executable(test ${SOURCE_FILES})
target_link_libraries(test ARTSynchronized)