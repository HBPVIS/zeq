# Copyright (c) HBP 2014-2016 Daniel.Nachbaur@epfl.ch
#                             Stefan.Eilemann@epfl.ch

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(ZeroEQ VERSION 0.7.0)
set(ZeroEQ_VERSION_ABI 6)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake
                              ${CMAKE_SOURCE_DIR}/CMake/common)
include(GitExternal)

set(ZEROEQ_DEB_DEPENDS libboost-test-dev libzmq3-dev)

include(Common)

set(COMMON_PROJECT_DOMAIN eu.humanbrainproject)
set(ZEROEQ_DESCRIPTION "ZeroEQ - Zero Event Queue")
set(ZEROEQ_MAINTAINER "Human Brain Project <HBPVis@googlegroups.com>")
set(ZEROEQ_LICENSE LGPL)
set(ZEROEQ_DEPENDENT_LIBRARIES Servus)

common_find_package(Boost COMPONENTS unit_test_framework)
common_find_package(httpxx)
common_find_package(libzmq REQUIRED)
common_find_package(Servus REQUIRED)
common_find_package(Threads REQUIRED)
common_find_package_post()

set(LCOV_EXCLUDE "zeroeq/http/jsoncpp/*")

add_subdirectory(zeroeq)
add_subdirectory(tests)

set(DOXYGEN_MAINPAGE_MD README.md)
set(DOXYGEN_EXTRA_INPUT ${PROJECT_SOURCE_DIR}/README.md)
include(CPackConfig)
include(DoxygenRule)
