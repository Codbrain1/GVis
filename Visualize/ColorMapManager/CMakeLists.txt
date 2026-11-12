include(GenerateExportHeader)

add_library(ColorMapManager SHARED)
add_library(GVis::VisualizeColorMapManager ALIAS ColorMapManager)
generate_export_header(ColorMapManager)

target_sources(ColorMapManager PRIVATE
    ColorMapManager.h
    ColorMapManager.cpp
    ColorMapTexture.h
    ColorMapTexture.cpp
    ColorMapManagerSerializer.h
    ColorMapManagerSerializer.cpp
)
target_include_directories(ColorMapManager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(ColorMapManager PUBLIC
            project_config
            GVis::Common
            Qt6::Core
            Qt6::Widgets
            Qt6::Gui
            Qt6::OpenGL
)