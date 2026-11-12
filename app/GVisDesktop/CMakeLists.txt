# 1. Настройка переводов
#------------------------------------------
set(TS_FILES
    ${PROJECT_SOURCE_DIR}/app/translates/GVis_ru.ts
)
#добавление файлов перевода
qt6_add_translations(
    GVis
    TS_FILES ${TS_FILES}
RESOURCE_PREFIX "/i18n"
)

#создание исполнимого qt файла
qt_add_executable(GVis
    MANUAL_FINALIZATION
    main.cpp
    ${PROJECT_SOURCE_DIR}/app/resources/icons.qrc
    ${PROJECT_SOURCE_DIR}/app/resources/styles.qrc
    ${PROJECT_SOURCE_DIR}/app/resources/shaders.qrc
    ${QM_FILES}
)

target_link_libraries(GVis PRIVATE
            project_config
            GVis::Common
            GVis::Core
            GVis::IO
            GVis::Visualize
            GVis::Models
            # Charting
            # Filtering
            # Physics
            # Remote
            # GVis::Session
            GVis::UI
            qcustomplot
            Qt6::Core
            Qt6::Widgets
            Qt6::Gui
            # Qt6::Quick
            # Qt6::Qml
            VTK::GUISupportQt
            VTK::CommonColor
            VTK::ImagingHybrid 
            VTK::RenderingVolumeOpenGL2
            VTK::RenderingOpenGL2
            VTK::InteractionStyle
            OpenMP::OpenMP_CXX
)
# Генерация макросов инициализации фабрик
vtk_module_autoinit(
    TARGETS GVis
    MODULES ${VTK_LIBRARIES}
)

if(WIN32) # на виндовс только gui
    set_target_properties(GVis PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    target_sources(GVis PRIVATE ${PROJECT_SOURCE_DIR}/app/resources/app_icon.rc)
    get_filename_component(VTK_INSTALL_DIR "${VTK_DIR}/../../.." ABSOLUTE)
    set(VTK_BIN_DIR "${VTK_INSTALL_DIR}/bin")

    add_custom_command(TARGET GVis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
    "${VTK_BIN_DIR}" 
    "$<TARGET_FILE_DIR:GVis>"
    COMMENT "Copying VTK binaries..."
    )
    add_custom_command(TARGET GVis POST_BUILD
    COMMAND ${QT_DIR}/bin/windeployqt.exe
    --compiler-runtime
        $<TARGET_FILE:GVis>
            COMMENT "Running windeployqt..."
            )
endif()
        
include(GNUInstallDirs)
install(TARGETS GVis
BUNDLE DESTINATION .
LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" 
DESTINATION bin 
FILES_MATCHING 
PATTERN "*.dll"        # Забираем библиотеки
PATTERN "platforms/*"  # Забираем критический плагин qwindows.dll
PATTERN "styles/*"
PATTERN "imageformats/*"
)
qt_finalize_executable(GVis)