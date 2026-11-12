add_library(StyleManager STATIC)
target_sources(StyleManager PRIVATE
            StyleManager.h
            StyleManager.cpp
)
target_include_directories(StyleManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(StyleManager PUBLIC
            project_config
            LayerManager
            ViewManager
            GVis::Visualize
            GVis::Common
            Qt6::Core
) 