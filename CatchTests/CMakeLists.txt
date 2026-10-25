cmake_minimum_required(VERSION 3.16)

project(CatchTests LANGUAGES CXX)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(APP_DIR ${CMAKE_SOURCE_DIR}/src/app)
set(MARKDOWN_DIR ${CMAKE_SOURCE_DIR}/src/markdown)
set(NAVIGATION_DIR ${CMAKE_SOURCE_DIR}/src/navigation)
set(SEARCH_DIR ${CMAKE_SOURCE_DIR}/src/search)
set(SETTINGS_DIR ${CMAKE_SOURCE_DIR}/src/settings)
set(WIDGETS_DIR ${CMAKE_SOURCE_DIR}/src/widgets)

set(CMAKE_AUTOUIC_SEARCH_PATHS ${APP_DIR} ${SETTINGS_DIR})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Gui Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Gui Test)

# ---------------------------------------------------------------------------
# Catch2 v3 dependency.
#
# Prefer a Catch2 v3 installation that CMake can already find (system package,
# vcpkg, CMAKE_PREFIX_PATH, ...). If none is available, fetch a pinned release
# automatically so a fresh clone can configure and build the tests without any
# manual setup step.
#
# Note: find_package() is intentionally NOT REQUIRED here, so that the fallback
# below can run. A missing Catch2 that cannot be downloaded still fails loudly
# through FetchContent.
# ---------------------------------------------------------------------------
set(REMINI_CATCH2_TAG v3.7.1)

find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
    message(STATUS "Catch2 v3 not found; fetching ${REMINI_CATCH2_TAG}")

    include(FetchContent)

    # Keep Catch2's own tests, docs, and extras out of the build.
    set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    set(CATCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG ${REMINI_CATCH2_TAG}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(Catch2)
else()
    message(STATUS "Using installed Catch2 ${Catch2_VERSION}")
endif()

add_executable(${PROJECT_NAME}
    main.cpp
    MkEdit_Tests.cpp
    mktextdocument_tests.cpp
    editcommand_tests.cpp
    NavigationView_Tests.cpp
    RecentFilesDialog_Tests.cpp

    ${MARKDOWN_DIR}/blockdata.cpp
    ${MARKDOWN_DIR}/blocksupport.h
    ${MARKDOWN_DIR}/blockrenderer.h
    ${MARKDOWN_DIR}/editcommand.cpp
    ${MARKDOWN_DIR}/linedata.cpp
    ${MARKDOWN_DIR}/formatdata.cpp
    ${MARKDOWN_DIR}/mkedit.cpp
    ${MARKDOWN_DIR}/markdownconnections.cpp
    ${MARKDOWN_DIR}/markdownparser.cpp
    ${MARKDOWN_DIR}/markdownrenderer.cpp
    ${MARKDOWN_DIR}/mktextdocument.cpp
    ${MARKDOWN_DIR}/highlighter.cpp
    ${APP_DIR}/views_handler.cpp
    ${NAVIGATION_DIR}/navigationview.cpp
    ${NAVIGATION_DIR}/navigationmodel.cpp
    ${WIDGETS_DIR}/recentfilesdialog.cpp
    ${SETTINGS_DIR}/settingsdialog.cpp
    ${WIDGETS_DIR}/togglebutton.cpp
    ${SEARCH_DIR}/textsearchworker.cpp
    ${APP_DIR}/mainwindow.ui
    ${SETTINGS_DIR}/settingsdialog.ui
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${APP_DIR}
    ${MARKDOWN_DIR}
    ${NAVIGATION_DIR}
    ${SEARCH_DIR}
    ${SETTINGS_DIR}
    ${WIDGETS_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
    Catch2::Catch2
)
