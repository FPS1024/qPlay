add_library(quarktv_mpv STATIC
    mpv_session.cpp
    mpv_session.h
)

target_include_directories(quarktv_mpv PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quarktv_mpv PUBLIC Qt6::Core Qt6::Gui PkgConfig::MPV)
