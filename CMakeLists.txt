cmake_minimum_required(VERSION 3.5)
project(Empirical)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# C++14
set(CMAKE_CXX_STANDARD 14)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)
if(NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Debug;MinSizeRel)
endif(NOT CMAKE_CONFIGURATION_TYPES)

# set-up ANTLR
add_definitions(-DANTLR4CPP_STATIC)
set(ANTLR_EXECUTABLE ${PROJECT_SOURCE_DIR}/thirdparty/antlr/antlr-4.7.2-complete.jar)
set(ANTLR4_ZIP_REPOSITORY ${PROJECT_SOURCE_DIR}/thirdparty/antlr/antlr4-master.zip)
include(ExternalAntlr4Cpp)
include_directories(${ANTLR4_INCLUDE_DIRS})

# generate ANTLR files
find_package(ANTLR REQUIRED)
antlr_target(Empirical
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Empirical.g4
    PACKAGE empirical
    VISITOR)
include_directories(${ANTLR_Empirical_OUTPUT_DIR})
antlr_target(VVMAsm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/VVM/VVMAsm.g4
    PACKAGE vvmasm
    VISITOR)
include_directories(${ANTLR_VVMAsm_OUTPUT_DIR})

# generate ASDL files
find_package(ASDL)
ASDL_TARGET(AST ${CMAKE_CURRENT_SOURCE_DIR}/src/AST.asdl
    ${CMAKE_CURRENT_BINARY_DIR}/AST.h
    ${CMAKE_CURRENT_BINARY_DIR}/AST.cpp)
ASDL_TARGET(HIR ${CMAKE_CURRENT_SOURCE_DIR}/src/HIR.asdl
    ${CMAKE_CURRENT_BINARY_DIR}/HIR.h
    ${CMAKE_CURRENT_BINARY_DIR}/HIR.cpp)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# generate VVM files
find_package(GenVVM)
GenVVM_TARGET(${CMAKE_CURRENT_BINARY_DIR}/VVM)

# system configurations
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
  execute_process(
    COMMAND git describe --tags --always
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE EMPIRICAL_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
if(UNIX AND NOT APPLE)
  OPTION(LINUX "GNU/Linux" ON)
endif(UNIX AND NOT APPLE)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/sysconfig.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.hpp
  @ONLY)

# static build for Visual Studio
# https://gitlab.kitware.com/cmake/community/wikis/FAQ#how-can-i-build-my-msvc-application-with-a-static-runtime
if(MSVC)
  foreach(flag_var
          CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endif(${flag_var} MATCHES "/MD")
  endforeach(flag_var)
endif(MSVC)

# build empirical
if(NOT WIN32)
    set(LINENOISE thirdparty/linenoise/linenoise.cpp)
endif(NOT WIN32)
set(DOCOPT thirdparty/docopt/docopt.cpp)
file(GLOB STRTIME "thirdparty/strtime/*.c")
file(GLOB EMPIRICAL_SRC "src/*.cpp")
file(GLOB VVM_SRC "src/VVM/*.cpp")
file(GLOB UTILS_SRC "src/VVM/utils/*.cpp")
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)
add_executable(empirical ${EMPIRICAL_SRC} ${VVM_SRC} ${UTILS_SRC}
    ${LINENOISE} ${DOCOPT} ${STRTIME}
    ${ANTLR_Empirical_CXX_OUTPUTS}
    ${ANTLR_VVMAsm_CXX_OUTPUTS}
    ${ASDL_AST_OUTPUTS} ${ASDL_HIR_OUTPUTS}
    ${GenVVM_OUTPUTS})
target_link_libraries(empirical antlr4_static)

# regression tests
enable_testing()
add_subdirectory(doc)
add_subdirectory(tests)
add_subdirectory(tests/VVM)
add_subdirectory(tests/VVM/utils)
