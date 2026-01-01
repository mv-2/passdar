cmake_minimum_required(VERSION 3.23)
project(passdar)
set(CMAKE_CXX_STANDARD 23)

# Project source files
set(SOURCES
  src/main.cpp
  src/cfgInterface.cpp
  src/radarData.cpp
  src/receiverIq.cpp
  src/sdrCapture.cpp
  src/spectrumData.cpp
)
add_executable(passdar ${SOURCES})

# JsonCpp
find_package(jsoncpp REQUIRED)

# MATPLOT
add_subdirectory(external/matplotplusplus)

target_link_libraries(passdar matplot JsonCpp::JsonCpp)

## ========================= FFTW3 & SDRPLAY INCLUSION =========================
set(FFTW_ROOT "/usr/local/x86_64-linux-gnu")
find_path(FFTW_INCLUDE_DIR NAMES fftw3.h PATHS ${FFTW_ROOT}/include)
find_library(FFTW_LIBRARY NAMES fftw3 PATHS ${FFTW_ROOT}/lib)
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
    ${FFTW_INCLUDE_DIR}
    ${SDRPLAY_INCLUDE_DIR}
)

# Link the library to your executable
target_link_libraries(passdar matplot ${FFTW_LIBRARY} ${SDRPLAY_LIBRARY})

