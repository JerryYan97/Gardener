cmake_minimum_required(VERSION 3.20)
project(WorkWaitWork)

set(CMAKE_CXX_STANDARD 14)

include_directories(${BOOST_ROOT})

set(SOURCE main.cpp
           ../../src/Gardener.h
           ../../src/Gardener.cpp
           ../../src/GardenerUtils.h
           ../../src/GardenerUtils.cpp)

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost 1.81.0 REQUIRED COMPONENTS
             fiber context)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    include_directories(../../src)
    add_executable(WorkWaitWork ${SOURCE})
    target_link_libraries(WorkWaitWork Boost::fiber Boost::context)
endif()