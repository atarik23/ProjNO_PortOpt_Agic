set(PORTFOLIO_APP_NAME PortfolioOptimizer)
set(PORTFOLIO_TEST_NAME PortfolioCoreTests)

set(PORTFOLIO_CORE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/core/CsvReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/core/Statistics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/numerics/ActiveSetQPSolver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/numerics/MatrixBackend.cpp
)

set(PORTFOLIO_CORE_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/src/core/PortfolioData.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/CsvReader.h
    ${CMAKE_CURRENT_LIST_DIR}/src/core/Statistics.h
    ${CMAKE_CURRENT_LIST_DIR}/src/numerics/ActiveSetQPSolver.h
    ${CMAKE_CURRENT_LIST_DIR}/src/numerics/MatrixBackend.h
)

add_library(portfolio_core STATIC
    ${PORTFOLIO_CORE_SOURCES}
    ${PORTFOLIO_CORE_HEADERS}
)

target_include_directories(
    portfolio_core
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src
)

target_compile_features(portfolio_core PUBLIC cxx_std_20)

target_link_libraries(portfolio_core
    PUBLIC
    debug ${MU_LIB_DEBUG}
    debug ${MATRIX_LIB_DEBUG}
    optimized ${MU_LIB_RELEASE}
    optimized ${MATRIX_LIB_RELEASE}
)

file(GLOB_RECURSE PORTFOLIO_APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
)
file(GLOB_RECURSE PORTFOLIO_APP_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/*.h
)

list(REMOVE_ITEM PORTFOLIO_APP_SOURCES ${PORTFOLIO_CORE_SOURCES})

set(PORTFOLIO_PLIST ${CMAKE_CURRENT_LIST_DIR}/res/appIcon/AppIcon.plist)

add_executable(${PORTFOLIO_APP_NAME}
    ${PORTFOLIO_APP_SOURCES}
    ${PORTFOLIO_APP_HEADERS}
)

target_include_directories(${PORTFOLIO_APP_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(${PORTFOLIO_APP_NAME}
    PRIVATE portfolio_core
    debug ${MU_LIB_DEBUG}
    debug ${NATGUI_LIB_DEBUG}
    debug ${MATRIX_LIB_DEBUG}
    optimized ${MU_LIB_RELEASE}
    optimized ${NATGUI_LIB_RELEASE}
    optimized ${MATRIX_LIB_RELEASE}
)

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src PREFIX "src" FILES
    ${PORTFOLIO_APP_SOURCES}
    ${PORTFOLIO_APP_HEADERS}
)

setTargetPropertiesForGUIApp(${PORTFOLIO_APP_NAME} ${PORTFOLIO_PLIST})
setIDEPropertiesForGUIExecutable(${PORTFOLIO_APP_NAME} ${CMAKE_CURRENT_LIST_DIR})
setPlatformDLLPath(${PORTFOLIO_APP_NAME})

add_executable(${PORTFOLIO_TEST_NAME} ${CMAKE_CURRENT_LIST_DIR}/tests/core_tests.cpp)
target_link_libraries(${PORTFOLIO_TEST_NAME} PRIVATE portfolio_core)
target_include_directories(${PORTFOLIO_TEST_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
setIDEPropertiesForExecutable(${PORTFOLIO_TEST_NAME})
setPlatformDLLPath(${PORTFOLIO_TEST_NAME})

enable_testing()
add_test(NAME portfolio_core_tests
         COMMAND ${PORTFOLIO_TEST_NAME} ${CMAKE_CURRENT_LIST_DIR}/res/data/sample_returns.csv)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PORTFOLIO_APP_NAME})
