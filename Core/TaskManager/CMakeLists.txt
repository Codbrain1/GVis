add_library(TaskManager STATIC)
target_sources(TaskManager PRIVATE 
            TaskManager.h
            TaskManager.cpp
)
target_include_directories(TaskManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(TaskManager PUBLIC
    project_config
    GVis::Common
    GVis::IO
    Qt6::Core
    Qt6::Concurrent
)