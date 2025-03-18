cmake_minimum_required(VERSION 3.14.4)

project(poco-example)

set(CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE PATH "The path to the cmake directory")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_DIR})

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2")

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "$ENV{CFLAGS} -Wall -Wextra -Werror")
    set(CMAKE_C_FLAGS_DEBUG
        "${CMAKE_C_FLAGS} -g -O0 -fsanitize=address \
        -fsanitize-recover=address -fsanitize-address-use-after-scope \
        -fno-stack-protector -fno-omit-frame-pointer -fno-var-tracking")
    set(CMAKE_CXX_FLAGS_DEBUG
        "-g -O0 -fsanitize=address \
        -fsanitize-recover=address -fsanitize-address-use-after-scope \
        -fno-stack-protector -fno-omit-frame-pointer -fno-var-tracking")
endif()

if(WIN32 OR MSVC)
    set(CMAKE_PREFIX_PATH "c:/locald22")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

    include_directories(${CMAKE_PREFIX_PATH}/include)
    link_directories(${CMAKE_PREFIX_PATH}/lib)
endif()

find_package(Poco REQUIRED COMPONENTS Foundation)

find_package(cppzmq 4.10.0 REQUIRED)

if(cppzmq_FOUND)
    message(STATUS "cppzmq_INCLUDE_DIR=${cppzmq_INCLUDE_DIR}")
    include_directories(${cppzmq_INCLUDE_DIR})
endif()

find_package(flatbuffers REQUIRED)

add_executable(pocoex main.cpp)
target_link_libraries(pocoex PRIVATE cppzmq-static)
target_link_libraries(pocoex PRIVATE Poco::Foundation)

target_link_directories(pocoex PRIVATE flatbuffers)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(COPY ${CMAKE_CURRENT_LIST_DIR}/pocoex.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
endif()