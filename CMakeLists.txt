cmake_minimum_required(VERSION 3.23)
project(passdar)
set(CMAKE_CXX_STANDARD 23)

# Project source files
set(SOURCE_DIR src)
set(HARDWARE_DIR hardwareInterface)
set(FRONTEND_DIR frontend)
set(PROCESSING_DIR processing)
set(UTIL_DIR util)
set(SOURCES
  ${SOURCE_DIR}/main.cpp
  ${SOURCE_DIR}/${FRONTEND_DIR}/radarApp.cpp
  ${SOURCE_DIR}/${HARDWARE_DIR}//cfgInterface.cpp
  ${SOURCE_DIR}/${PROCESSING_DIR}/radarData.cpp
  ${SOURCE_DIR}/${HARDWARE_DIR}/receiverIq.cpp
  ${SOURCE_DIR}/${HARDWARE_DIR}/sdrCapture.cpp
  ${SOURCE_DIR}/${PROCESSING_DIR}/spectrumData.cpp
  ${SOURCE_DIR}/${UTIL_DIR}/leftRightData.h
)
add_executable(passdar ${SOURCES})

find_path(SDRPLAY_INCLUDE_DIR sdrplay_api.h
  PATHS /usr/local/include /usr/include
  NO_CMAKE_FIND_ROOT_PATH
)
find_library(SDRPLAY_LIBRARY NAMES sdrplay_api libsdrplay_api.so
  PATHS /usr/local/lib /usr.lib
  NO_CMAKE_FIND_ROOT_PATH
)
target_include_directories(passdar
  PRIVATE
    ${SDRPLAY_INCLUDE_DIR}
)
target_link_libraries(passdar PUBLIC ${SDRPLAY_LIBRARY})
find_package(FFTW3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(passdar PRIVATE
  FFTW3::fftw3
  imgui::imgui
  implot::implot
  OpenGL::GL
  glfw
  nlohmann_json::nlohmann_json
)
