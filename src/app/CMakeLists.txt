qt_add_library(quarktv_app_core STATIC
    player_engine.cpp
    player_engine.h
)

target_include_directories(quarktv_app_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quarktv_app_core
    PUBLIC
        quarktv_core
        quarktv_library
        quarktv_mpv
        quarktv_video
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Widgets
)

qt_add_executable(quarktv
    main.cpp
)

qt_add_resources(quarktv "qplay_icon"
    PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/assets/icons"
    FILES "${PROJECT_SOURCE_DIR}/assets/icons/qPlay.png"
)

target_link_libraries(quarktv PRIVATE quarktv_app_core Qt6::Quick Qt6::QuickControls2)

qt_add_resources(quarktv "quarktv_qml"
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/qml"
    FILES
        "${PROJECT_SOURCE_DIR}/qml/Main.qml"
        "${PROJECT_SOURCE_DIR}/qml/components/PlayerControls.qml"
        "${PROJECT_SOURCE_DIR}/qml/components/SettingsDialog.qml"
        "${PROJECT_SOURCE_DIR}/qml/components/TrackMenu.qml"
        "${PROJECT_SOURCE_DIR}/qml/components/PosterWall.qml"
        "${PROJECT_SOURCE_DIR}/qml/components/TitleDetails.qml"
)

set_target_properties(quarktv PROPERTIES
    OUTPUT_NAME qPlay
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)
