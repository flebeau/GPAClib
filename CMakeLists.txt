cmake_minimum_required( VERSION 3.1 )

project(GPACsim)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall")

#package
find_package(Boost COMPONENTS program_options system iostreams REQUIRED)
find_package(OpenMP)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

include_directories(src ${Boost_INCLUDE_DIR})

file(GLOB SOURCES src/*.cpp)

add_executable(GPACsim ${SOURCES})
target_link_libraries(GPACsim ${Boost_LIBRARIES})
