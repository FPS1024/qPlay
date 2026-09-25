add_library(quarktv_core STATIC
    media_types.h
)

target_include_directories(quarktv_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(quarktv_core PUBLIC Qt6::Core)
