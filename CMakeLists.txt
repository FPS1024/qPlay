cmake_minimum_required(VERSION 3.22)

project(qPlay
    VERSION 0.1.1
    DESCRIPTION "qPlay desktop media library and player powered by Qt 6 and libmpv"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Network
    Sql
    Widgets
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

add_subdirectory(src)

install(TARGETS quarktv
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
