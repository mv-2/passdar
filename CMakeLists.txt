cmake_minimum_required(VERSION 3.23)
project(passdar)
set(CMAKE_CXX_STANDARD 23)

# Comppile options
add_compile_options(-O3 -Wall -Wextra -pedantic-errors -Werror)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# Project source files
set(SOURCES
  src/main.cpp
  src/radarApp.cpp
  src/cfgInterface.cpp
  src/radarData.cpp
  src/receiverIq.cpp
  src/sdrCapture.cpp
  src/spectrumData.cpp
)
add_executable(passdar ${SOURCES})

# nlohmann_json
target_link_libraries(passdar PRIVATE nlohmann_json)

# FFTW
set(FFTW_ROOT "/usr/local/x86_64-linux-gnu")
find_path(FFTW_INCLUDE_DIR NAMES fftw3.h PATHS ${FFTW_ROOT}/include)
find_library(FFTW_LIBRARY NAMES fftw3 PATHS ${FFTW_ROOT}/lib)
target_include_directories(passdar
  PRIVATE
    ${FFTW_INCLUDE_DIR}
)
target_link_libraries(passdar PUBLIC ${FFTW_LIBRARY})

# SDRPLAY
find_path(SDRPLAY_INCLUDE_DIR sdrplay_api.h
    PATHS /usr/local/include /usr/include
    NO_CMAKE_FIND_ROOT_PATH
)
find_library(SDRPLAY_LIBRARY NAMES sdrplay_api libsdrplay_api.so
    PATHS /usr/local/lib /usr/lib
    NO_CMAKE_FIND_ROOT_PATH
)
target_include_directories(passdar
  PRIVATE
    ${SDRPLAY_INCLUDE_DIR}
)
target_link_libraries(passdar PUBLIC ${SDRPLAY_LIBRARY})

# Git submodules
add_subdirectory(external)
target_link_libraries(passdar PUBLIC imgui_core implot)
