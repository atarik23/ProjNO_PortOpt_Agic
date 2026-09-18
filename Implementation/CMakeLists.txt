cmake_minimum_required(VERSION 3.18)

set(SOLUTION_NAME PortfolioOptimization)
project(${SOLUTION_NAME})

set(SOURCE_ROOT ${CMAKE_CURRENT_LIST_DIR})

if(WIN32)
    set(HOME_ROOT $ENV{USERPROFILE})
    string(REPLACE "\\" "/" HOME_ROOT "${HOME_ROOT}")
else()
    set(HOME_ROOT $ENV{HOME})
endif()

if(NOT NATID_SDK_ROOT)
    set(NATID_SDK_ROOT ${HOME_ROOT}/natID.SDK)
endif()
file(TO_CMAKE_PATH "${NATID_SDK_ROOT}" NATID_SDK_ROOT)
set(NATID_SDK_ROOT "${NATID_SDK_ROOT}" CACHE PATH "Path to the natID SDK")

if(NOT EXISTS "${NATID_SDK_ROOT}/DevEnv/Common.cmake")
    message(FATAL_ERROR "natID SDK was not found at ${NATID_SDK_ROOT}")
endif()

include(${NATID_SDK_ROOT}/DevEnv/Common.cmake)
include(${NATID_SDK_ROOT}/DevEnv/natGUI.cmake)
include(${NATID_SDK_ROOT}/DevEnv/MatrixLib.cmake)

include(PortfolioOptimizer.cmake)
