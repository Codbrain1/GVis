add_compile_definitions(QT_NO_DEBUG_OUTPUT)
# --- Добавление новых тестов в песочницу
add_sandbox_test(StressTestOneReadFileStandart
    SOURCES 
        StressTestOneReadFileStandart/main.cpp 
    LIBS
        GVis::IO  
)
add_sandbox_test(StressTestOneReadFileMapped
    SOURCES 
        StressTestOneReadFileMapped/main.cpp 
    LIBS
        GVis::IO  
)
add_sandbox_test(StressTestBatchReadFileStandart
    SOURCES
        StressTestBatchReadFIleStandart/main.cpp
    LIBS
        GVis::IO
)
add_sandbox_test(StressTestBatchReadFileMapped
    SOURCES
        StressTestBatchReadFIleMapped/main.cpp
    LIBS
        GVis::IO
)
add_sandbox_test(StressTestVisualizeSPHData
    SOURCES
        StressTestVisualizeSPHData/main.cpp
        ${PROJECT_SOURCE_DIR}/app/resources/shaders.qrc
    LIBS
        GVis::Common
        GVis::Visualize
        GVis::IO
        VTK::CommonCore
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
)

if(WIN32)
    get_filename_component(VTK_INSTALL_DIR "${VTK_DIR}/../../.." ABSOLUTE)
    add_custom_command(TARGET StressTestVisualizeSPHData POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VTK_INSTALL_DIR}/bin"
            "$<TARGET_FILE_DIR:StressTestVisualizeSPHData>"
        COMMENT "Copying VTK DLLs for StressTestVisualizeSPHData"
    )
endif()
# add_sandbox_test(TestVtkVisualize  
#     SOURCES
#         test_vtkvisualize/main.cpp 
#     LIBS
#         GVis::Visualize GVis::IO VTK::CommonCore
#         Qt6::3DCore
#         Qt6::3DExtras
#         Qt6::3DInput
#         Qt6::3DRender
#         Qt6::Core
#         Qt6::Gui
#         Qt6::Widgets
#             )
# add_sandbox_test(TestSPHVisualize  
#     SOURCES
#         TestSPHVisualize/main.cpp 
#     LIBS
#         GVis::Common GVis::Visualize GVis::IO VTK::CommonCore
#         Qt6::Core
#         Qt6::Gui
#         Qt6::Widgets
#         Qt6::OpenGL
#         Qt6::OpenGLWidgets
#             )

# add_sandbox_test(TestCONV  
#     SOURCES
#         testCONV/main.cpp 
#     LIBS
#         GVis::Visualize GVis::IO VTK::CommonCore
#         Qt6::OpenGL
#         Qt6::OpenGLWidgets
#         Qt6::Core
#         Qt6::Gui
#         Qt6::Widgets
#             )

