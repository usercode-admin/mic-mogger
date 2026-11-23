cmake_minimum_required(VERSION 3.20)
project(BuffMic LANGUAGES CXX C RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---- GLFW: vẫn dùng FetchContent, CMake tự chọn Win32 backend vì
#      CMAKE_SYSTEM_NAME=Windows trong toolchain file.
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.9)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.90.4)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 0.11.21)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
  FetchContent_Populate(miniaudio)
endif()

set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_SOURCE_DIR}/resources")
add_executable(BuffMic WIN32
  src/main.cpp
  src/audio_engine.cpp
  src/driver.cpp
  src/ui.cpp
  src/installer.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  resources/app.rc
)

target_include_directories(BuffMic PRIVATE
  src
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${miniaudio_SOURCE_DIR}
)

target_compile_definitions(BuffMic PRIVATE UNICODE _UNICODE NOMINMAX)

# ---- FIX: không dùng find_package(OpenGL), link thẳng opengl32
#          + comdlg32 (GetOpenFileNameA) + gdi32 (GLFW Win32 backend)
target_link_libraries(BuffMic PRIVATE
  glfw
  opengl32
  gdi32
  comdlg32
  ole32 shell32 advapi32 shlwapi setupapi winmm user32 kernel32
)

# ---- FIX: bỏ /MANIFESTUAC (MSVC-only). Manifest được nhúng qua app.rc.
#      Static runtime để exe độc lập.
if(MINGW)
  target_link_options(BuffMic PRIVATE
    -static-libgcc
    -static-libstdc++
    -static
  )
endif()

set_target_properties(BuffMic PROPERTIES OUTPUT_NAME "BuffMic")
