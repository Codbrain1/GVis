if(ENABLE_TESTS)
    add_subdirectory(Testing)
endif()

add_library(SessionManager STATIC)

target_sources(SessionManager PRIVATE 
SessionManager.h
SessionManager.cpp
)
target_include_directories(SessionManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(SessionManager PUBLIC
            project_config
            GVis::CoreObjectRegistry
            GVis::CoreLayerManager
            GVis::CoreViewManager
            GVis::Session
            GVis::Visualize
            GVis::Common
            Qt6::Core
)