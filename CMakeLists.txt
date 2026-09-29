cmake_minimum_required(VERSION 3.22)

project(qPlay
    VERSION 1.0.0
    DESCRIPTION "qPlay desktop media library and player powered by Qt 6 and libmpv"
    LANGUAGES CXX
)

include(GNUInstallDirs)

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

install(FILES packaging/qplay.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES assets/icons/qPlay.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
    RENAME qplay.png
)
