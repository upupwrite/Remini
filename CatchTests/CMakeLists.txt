cmake_minimum_required(VERSION 3.5)

project(CatchTests LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Gui Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Gui Test)

# Catch2 v3 (Ubuntu 24.04) or v2 (fetch via CATCH_INCLUDE_DIR).
find_package(Catch2 3 QUIET)

set(TEST_SOURCES
    MkEdit_Tests.cpp
    mktextdocument_tests.cpp
    NavigationView_Tests.cpp
    RecentFilesDialog_Tests.cpp
)

# Every .cpp from the main project that the tests (or their dependencies)
# actually reference at link time.
set(PROJECT_SOURCES
    ${CMAKE_SOURCE_DIR}/blockdata.cpp
    ${CMAKE_SOURCE_DIR}/linedata.cpp
    ${CMAKE_SOURCE_DIR}/formatdata.cpp
    ${CMAKE_SOURCE_DIR}/mkedit.cpp
    ${CMAKE_SOURCE_DIR}/mktextdocument.cpp
    ${CMAKE_SOURCE_DIR}/highlighter.cpp
    ${CMAKE_SOURCE_DIR}/views_handler.cpp
    ${CMAKE_SOURCE_DIR}/navigationview.cpp
    ${CMAKE_SOURCE_DIR}/navigationmodel.cpp
    ${CMAKE_SOURCE_DIR}/recentfilesdialog.cpp
    ${CMAKE_SOURCE_DIR}/settingsdialog.cpp
    ${CMAKE_SOURCE_DIR}/textsearchworker.cpp      # ADD
    ${CMAKE_SOURCE_DIR}/togglebutton.cpp          # ADD
)

if(Catch2_FOUND)
    # Catch2 v3: header + library. main() is provided by Catch2WithMain.
    add_executable(${PROJECT_NAME}
        ${TEST_SOURCES}
        ${PROJECT_SOURCES}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Catch2::Catch2WithMain
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
    )
else()
    # Catch2 v2: header-only, main() lives in main.cpp.
    add_executable(${PROJECT_NAME}
        main.cpp
        ${TEST_SOURCES}
        ${PROJECT_SOURCES}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Catch2 v2 needs CATCH_INCLUDE_DIR from the environment / Qt Creator wizard.
if (DEFINED ENV{CATCH_INCLUDE_DIR})
    set(CATCH_INCLUDE_DIR $ENV{CATCH_INCLUDE_DIR})
else ()
    set(CATCH_INCLUDE_DIR "")
endif ()

if (CATCH_INCLUDE_DIR STREQUAL "")
    message("CATCH_INCLUDE_DIR is not set, assuming Catch2 can be found automatically in your system")
elseif (EXISTS ${CATCH_INCLUDE_DIR})
    target_include_directories(${PROJECT_NAME} PRIVATE ${CATCH_INCLUDE_DIR})
endif ()
