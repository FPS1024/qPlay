add_library(quarktv_video STATIC
    mpv_video_item.cpp
    mpv_video_item.h
)

target_include_directories(quarktv_video PUBLIC ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quarktv_video
    PUBLIC
        Qt6::Quick
        PkgConfig::MPV
)
