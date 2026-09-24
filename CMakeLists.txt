cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/SniperElite3.FreeCamera/Version.h" version_define
    REGEX "^#define CAMERA_TWEAKS_VERSION ")
string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" CAMERA_TWEAKS_VERSION "${version_define}")
project(SniperElite3CameraTweaks VERSION ${CAMERA_TWEAKS_VERSION} LANGUAGES C CXX RC)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Build for Windows x86 (Win32), not x64.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/minhook)
set(SRC SniperElite3.FreeCamera)
set(IMGUI third_party/imgui)
add_library(FreeCamera SHARED
    ${SRC}/dllmain.cpp ${SRC}/FreeCamera.cpp ${SRC}/core.cpp
    ${SRC}/SettingsMgr.cpp ${SRC}/IniReader.cpp
    ${SRC}/se3/Camera.cpp ${SRC}/se3/Matrix.cpp ${SRC}/se3/Vector.cpp
    ${SRC}/FovControl.cpp ${SRC}/FovOverlay.cpp ${SRC}/SniperElite3.FreeCamera.rc
    ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_win32.cpp ${IMGUI}/backends/imgui_impl_dx11.cpp)
target_include_directories(FreeCamera PRIVATE ${IMGUI} ${SRC})
target_compile_definitions(FreeCamera PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(FreeCamera PRIVATE minhook d3d11 dxgi d3dcompiler dwmapi user32 gdi32 imm32)
set_target_properties(FreeCamera PROPERTIES PREFIX "" OUTPUT_NAME "SniperElite3.CameraTweaks" SUFFIX ".asi")
if(MINGW)
    target_link_options(FreeCamera PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_custom_command(TARGET FreeCamera POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/SniperElite3.CameraTweaks.ini" "$<TARGET_FILE_DIR:FreeCamera>/SniperElite3.CameraTweaks.ini")
