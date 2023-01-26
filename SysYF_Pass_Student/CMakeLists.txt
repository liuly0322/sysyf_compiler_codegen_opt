cmake_minimum_required(VERSION 3.5)

project(SysYFCompiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()

option(BUILD_WASM "Build WASM output files" OFF)

if(BUILD_WASM)
  # WASM with -Oz
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Oz -Wall -pedantic")
elseif(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
  # release mode with -O2
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -pedantic")
else()
  # debug mode wich -O0 and -g
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -Wall -g -pedantic")
endif()

# add macros to generate Bison & Flex Cpp code from grammar
# find_package(BISON REQUIRED)
# find_package(FLEX REQUIRED)

# BISON_TARGET(SysYParser ${CMAKE_CURRENT_SOURCE_DIR}/grammar/SysYParser.yy ${CMAKE_CURRENT_BINARY_DIR}/SysYParser.cpp
#              COMPILE_FLAGS "-Wall -Wcounterexamples --defines=${CMAKE_CURRENT_BINARY_DIR}/SysYParser.h")
# FLEX_TARGET(SysYScanner ${CMAKE_CURRENT_SOURCE_DIR}/grammar/SysYScanner.ll  ${CMAKE_CURRENT_BINARY_DIR}/SysYScanner.cpp
#             COMPILE_FLAGS "--debug --c++")
# ADD_FLEX_BISON_DEPENDENCY(SysYScanner SysYParser)

# include generated files in project environment
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/ASTBuilder)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/ASTPrinter)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/Checker)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/ErrorReporter)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/Frontend)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/SysYFIR)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/SysYFIRBuilder)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/Optimize)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/Log)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/ASTBuilder)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/ASTPrinter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/Checker)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/ErrorReporter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/Frontend)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/SysYFIR)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/SysYFIRBuilder)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/Optimize)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/Log)

if(BUILD_WASM)
  add_executable(
    compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm.cpp
  )
  target_link_options(compiler PRIVATE -sEXPORTED_FUNCTIONS=[_compile,_malloc,_free])
  target_link_libraries(
    compiler
    SysYFPass
    SysYFIRBuilder
    IRLib
    Driver
  )
else()
  add_executable(
    compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  )
  target_link_libraries(
    compiler
    SysYFPass
    SysYFIRBuilder
    IRLib
    Driver
    Checker
    ASTPrinter
    ErrReporter
    Log
  )
  add_executable(
    test_logging
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Log/test_logging.cpp
  )
  target_link_libraries(
    test_logging
    Log
  )
endif()