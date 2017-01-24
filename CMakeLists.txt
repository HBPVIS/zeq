# Copyright (c) HBP 2014-2017 Daniel.Nachbaur@epfl.ch
#                             Stefan.Eilemann@epfl.ch

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(ZeroEQ VERSION 0.8.0)
set(ZeroEQ_VERSION_ABI 8)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake
                              ${CMAKE_SOURCE_DIR}/CMake/common)


option(ZEROEQ_USE_EXTERNAL_CPPNETLIB "For ZeroEQ to use cppnetlib as external dependency" OFF)

include(GitExternal)

set(ZEROEQ_DEB_DEPENDS
  libboost-test-dev
  libzmq3-dev
  # cppnetlib dependencies
  libboost-atomic-dev
  libboost-chrono-dev
  libboost-date-time-dev
  libboost-filesystem-dev
  libboost-program-options-dev
  libboost-regex-dev
  libboost-system-dev
  libboost-thread-dev
  libssl-dev
)

include(Common)

set(COMMON_PROJECT_DOMAIN eu.humanbrainproject)
set(ZEROEQ_DESCRIPTION "ZeroEQ - Zero Event Queue")
set(ZEROEQ_MAINTAINER "Human Brain Project <HBPVis@googlegroups.com>")
set(ZEROEQ_LICENSE LGPL)
set(ZEROEQ_DEPENDENT_LIBRARIES Servus)


if(NOT ZEROEQ_USE_EXTERNAL_CPPNETLIB)
# enforce cloning our own version of cppnetlib and skip finding a potentially
# installed system package
git_external(${COMMON_SOURCE_DIR}/cppnetlib
  https://github.com/BlueBrain/cpp-netlib 0.11-bbp)
add_subproject(cppnetlib)


common_find_package(cppnetlib SYSTEM NO_DEFAULT_PATH)

else()

common_find_package(cppnetlib REQUIRED)

endif()

common_find_package(Boost COMPONENTS unit_test_framework)
common_find_package(libzmq 4 REQUIRED)
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
