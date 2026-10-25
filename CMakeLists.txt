cmake_minimum_required(VERSION 3.16)

project(Remini VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# The global-hotkey implementation uses X11 only on desktop Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
endif()

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
set(MARKDOWN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/markdown)
set(NAVIGATION_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/navigation)
set(SEARCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/search)
set(SETTINGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/settings)
set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/platform)
set(WIDGETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets)

# Set this before targets are created; CMake copies it into each target's
# AUTOUIC_SEARCH_PATHS property when the target is initialized.
set(CMAKE_AUTOUIC_SEARCH_PATHS ${APP_DIR} ${SETTINGS_DIR})

set(PROJECT_SOURCES
    ${APP_DIR}/main.cpp
    ${APP_DIR}/mainwindow.cpp
    ${APP_DIR}/mainwindow.h
    ${APP_DIR}/mainwindow.ui
    ${APP_DIR}/views_handler.cpp
    ${APP_DIR}/views_handler.h
    ${APP_DIR}/appeventfilter.cpp
    ${APP_DIR}/appeventfilter.h
    ${APP_DIR}/startuptext.h
    ${APP_DIR}/theme.h

    ${MARKDOWN_DIR}/mkedit.cpp
    ${MARKDOWN_DIR}/mkedit.h
    ${MARKDOWN_DIR}/highlighter.cpp
    ${MARKDOWN_DIR}/highlighter.h
    ${MARKDOWN_DIR}/markdownparser.cpp
    ${MARKDOWN_DIR}/markdownparser.h
    ${MARKDOWN_DIR}/markdownrenderer.cpp
    ${MARKDOWN_DIR}/markdownrenderer.h
    ${MARKDOWN_DIR}/mktextdocument.cpp
    ${MARKDOWN_DIR}/mktextdocument.h
    ${MARKDOWN_DIR}/blockdata.cpp
    ${MARKDOWN_DIR}/blockdata.h
    ${MARKDOWN_DIR}/blocksupport.h
    ${MARKDOWN_DIR}/blockrenderer.h
    ${MARKDOWN_DIR}/linedata.cpp
    ${MARKDOWN_DIR}/linedata.h
    ${MARKDOWN_DIR}/formatdata.cpp
    ${MARKDOWN_DIR}/formatdata.h

    ${NAVIGATION_DIR}/navigationmodel.cpp
    ${NAVIGATION_DIR}/navigationmodel.h
    ${NAVIGATION_DIR}/navigationview.cpp
    ${NAVIGATION_DIR}/navigationview.h

    ${SEARCH_DIR}/textsearchworker.cpp
    ${SEARCH_DIR}/textsearchworker.h

    ${SETTINGS_DIR}/settingsdialog.cpp
    ${SETTINGS_DIR}/settingsdialog.h
    ${SETTINGS_DIR}/settingsdialog.ui

    ${WIDGETS_DIR}/recentfilesdialog.cpp
    ${WIDGETS_DIR}/recentfilesdialog.h
    ${WIDGETS_DIR}/togglebutton.cpp
    ${WIDGETS_DIR}/togglebutton.h

    ${PLATFORM_DIR}/windowapi.cpp
    ${PLATFORM_DIR}/windowapi.h
    resources/icons.qrc
)

if(WIN32)
    list(APPEND PROJECT_SOURCES ${PLATFORM_DIR}/iwindowapi.h)
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(Remini MANUAL_FINALIZATION ${PROJECT_SOURCES})
elseif(ANDROID)
    add_library(Remini SHARED ${PROJECT_SOURCES})
else()
    add_executable(Remini ${PROJECT_SOURCES})
endif()

# Include the src root to preserve existing module-qualified includes such as
# "settings/settingsdialog.h", plus each module for concise local includes.
target_include_directories(Remini PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${APP_DIR}
    ${MARKDOWN_DIR}
    ${NAVIGATION_DIR}
    ${SEARCH_DIR}
    ${SETTINGS_DIR}
    ${PLATFORM_DIR}
    ${WIDGETS_DIR}
)

target_link_libraries(Remini PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Remini PRIVATE X11::X11)
endif()

enable_testing()
add_subdirectory(CatchTests)
add_test(NAME CatchTests COMMAND CatchTests)

set_target_properties(Remini PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Remini)
endif()
