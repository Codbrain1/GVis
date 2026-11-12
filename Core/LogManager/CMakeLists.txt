add_library(LogManager STATIC)
target_sources(LogManager PRIVATE
             LogManager.h
             LogManager.cpp
)
target_include_directories(LogManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(LogManager PUBLIC
            project_config
            GVis::Common
)