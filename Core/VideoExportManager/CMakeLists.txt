add_library(VideoExportManager STATIC)
target_sources(VideoExportManager PRIVATE
        VideoExportManager.h
        VideoExportManager.cpp
)
target_include_directories(VideoExportManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(VideoExportManager PUBLIC 
            project_config
            Qt6::Core
            GVis::Visualize
)   