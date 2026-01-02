cmake_minimum_required(VERSION 3.14)

# IMGUI
add_library(imgui_core
  STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
)
target_include_directories(imgui_core PUBLIC imgui)

# IMGUI BACKENDS
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
target_sources(imgui_core PUBLIC
  imgui/backends/imgui_impl_glfw.cpp
  imgui/backends/imgui_impl_opengl3.cpp
)
target_link_libraries(passdar
  PRIVATE
    imgui_core
    glfw
    OpenGL
)

# Implot
add_library(implot
  STATIC
    implot/implot.cpp
    implot/implot_internal.h
    implot/implot_items.cpp
    implot/implot_demo.cpp
)
target_include_directories(implot
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/implot
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)
target_link_libraries(implot PUBLIC imgui_core)
