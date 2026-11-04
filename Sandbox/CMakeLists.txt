add_compile_definitions(QT_NO_DEBUG_OUTPUT)
# --- Добавление новых тестов в песочницу
add_sandbox_test(StressTestOneReadFileStandart
    SOURCES 
        StressTestOneReadFileStandart/main.cpp 
    LIBS
        QSpace::IO  
)
add_sandbox_test(StressTestOneReadFileMapped
    SOURCES 
        StressTestOneReadFileMapped/main.cpp 
    LIBS
        QSpace::IO  
)
add_sandbox_test(StressTestBatchReadFileStandart
    SOURCES
        StressTestBatchReadFIleStandart/main.cpp
    LIBS
        QSpace::IO
)
add_sandbox_test(StressTestBatchReadFileMapped
    SOURCES
        StressTestBatchReadFIleMapped/main.cpp
    LIBS
        QSpace::IO
)
# add_sandbox_test(TestVtkVisualize  
#     SOURCES
#         test_vtkvisualize/main.cpp 
#     LIBS
#         QSpace::Visualize QSpace::IO VTK::CommonCore
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
#         QSpace::Common QSpace::Visualize QSpace::IO VTK::CommonCore
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
#         QSpace::Visualize QSpace::IO VTK::CommonCore
#         Qt6::OpenGL
#         Qt6::OpenGLWidgets
#         Qt6::Core
#         Qt6::Gui
#         Qt6::Widgets
#             )

