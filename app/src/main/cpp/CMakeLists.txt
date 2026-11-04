cmake_minimum_required(VERSION 3.22.1)
project(studio_engine LANGUAGES CXX)

add_library(studio_engine SHARED
    jni_bridge.cpp
    engine.cpp
    app.cpp
    effects.cpp
    effects_more.cpp
    export.cpp
    model/model.cpp
    model/anim.cpp
    model/scene.cpp
    model/storage.cpp
)

target_include_directories(studio_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(log-lib log)

target_link_libraries(studio_engine
    GLESv3
    EGL
    ${log-lib}
)
