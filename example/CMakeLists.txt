# CMakeList.txt : CMake project for LWSHelper, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("example")


if(MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
else(MSVC)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
endif(MSVC)

set(PROJECTLIB "../lib")
set(PROJECTDLL "../../../../dll")


include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")
include_directories("../include")
include_directories("..")
include_directories(${PROJECTLIB})

find_library(PROJECTLIB websockets)
IF (NOT PROJECTLIB)
    MESSAGE(FATAL_ERROR "libwebsockets not found")
ENDIF (NOT PROJECTLIB})
MESSAGE(STATUS PROJECTLIB " found libwebsockets")

find_library(PROJECTLIB ssl)
IF (NOT PROJECTLIB)
    MESSAGE(FATAL_ERROR "libssl not found")
ENDIF (NOT PROJECTLIB)
MESSAGE(STATUS PROJECTLIB " found libssl")

find_library(PROJECTLIB z)
IF (NOT PROJECTLIB)
    MESSAGE(FATAL_ERROR "zlib not found")
ENDIF (NOT PROJECTLIB)
MESSAGE(STATUS PROJECTLIB " found zlib")

LINK_DIRECTORIES(${PROJECTDLL})
LINK_DIRECTORIES(${PROJECTLIB})

# Add source to this project's executable.
add_executable (example "example.cpp")
TARGET_LINK_LIBRARIES(example websockets libssl zlib)

if (WIN32)
    add_custom_command(TARGET example POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECTDLL}              
    ${PROJECT_BINARY_DIR})
endif ()

# TODO: Add tests and install targets if needed.
