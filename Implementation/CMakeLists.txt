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

set(GUI_TARGET ${PORTFOLIO_APP_NAME})
if(APPLE AND NOT CMAKE_GENERATOR STREQUAL "Xcode" AND TARGET ${GUI_TARGET})
    set(PLIST_TOOL /usr/libexec/PlistBuddy)
    set(BUNDLE_PLIST $<TARGET_BUNDLE_DIR:${GUI_TARGET}>/Contents/Info.plist)
    add_custom_command(TARGET ${GUI_TARGET} POST_BUILD
        COMMAND ${PLIST_TOOL} -c "Set :CFBundleExecutable ${GUI_TARGET}" ${BUNDLE_PLIST}
        COMMAND ${PLIST_TOOL} -c "Set :CFBundleName ${GUI_TARGET}" ${BUNDLE_PLIST}
        COMMAND ${PLIST_TOOL} -c "Set :CFBundleIdentifier com.example.portfoliooptimizer" ${BUNDLE_PLIST}
        COMMAND ${PLIST_TOOL} -c "Set :CFBundlePackageType APPL" ${BUNDLE_PLIST}
        COMMAND ${PLIST_TOOL} -c "Set :CFBundleDevelopmentRegion en" ${BUNDLE_PLIST}
        COMMAND ${PLIST_TOOL} -c "Set :LSMinimumSystemVersion 12.3" ${BUNDLE_PLIST}
        COMMAND codesign --force --deep -s - $<TARGET_BUNDLE_DIR:${GUI_TARGET}>
        COMMENT "Fixing Info.plist and signing the app bundle"
    )
endif()
