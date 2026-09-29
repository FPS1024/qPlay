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

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "qPlay")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "qPlay")
set(CPACK_PACKAGE_CONTACT "qPlay maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native desktop media library and player powered by libmpv")
set(CPACK_PACKAGE_FILE_NAME "qPlay-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_DEBIAN_PACKAGE_NAME "qplay")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "qPlay maintainers")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.deb")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, libqt6sql6-sqlite")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)
