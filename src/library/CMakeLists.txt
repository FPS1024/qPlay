add_library(quarktv_library STATIC
    media_library.cpp
    media_library.h
)

target_include_directories(quarktv_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quarktv_library
    PUBLIC
        quarktv_core
        Qt6::Sql
)
