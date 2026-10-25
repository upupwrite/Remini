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
find_package(Catch2 3 REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    MkEdit_Tests.cpp
    mktextdocument_tests.cpp
    NavigationView_Tests.cpp
    RecentFilesDialog_Tests.cpp

    ${MARKDOWN_DIR}/blockdata.cpp
    ${MARKDOWN_DIR}/blocksupport.h
    ${MARKDOWN_DIR}/blockrenderer.h
    ${MARKDOWN_DIR}/linedata.cpp
    ${MARKDOWN_DIR}/formatdata.cpp
    ${MARKDOWN_DIR}/mkedit.cpp
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
