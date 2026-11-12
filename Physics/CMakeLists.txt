add_subdirectory(Math)
add_library(Physics INTERFACE)
add_library(GVis::Physics ALIAS Physics)
target_sources(Physics INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/DimensionConverter/PhysicalUnits.h
    # Если появятся еще хедеры, просто перечисляйте их ниже:
    # ${CMAKE_CURRENT_SOURCE_DIR}/ConservationLaws/SomeLaw.h
)
target_include_directories(Physics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(Physics INTERFACE
    project_config
    Math
)