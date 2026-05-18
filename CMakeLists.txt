cmake_minimum_required(VERSION 3.20)
project(jump_mouse VERSION 1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: static CRT linkage (/MT) — no VC redist needed on target machine
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# MSVC: treat source files as UTF-8
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
# UNICODE is required for Windows API wide-char support
add_definitions(-DUNICODE -D_UNICODE)

# ========== Daemon ==========
add_executable(jump_mouse_daemon WIN32
    src/daemon/daemon_main.cpp
    src/daemon/application.cpp
    src/daemon/focus_monitor.cpp
    src/daemon/key_tracker.cpp
    src/daemon/trigger_detector.cpp
    src/daemon/mouse_controller.cpp
    src/common/config_manager.cpp
    src/common/adaptation_layer.cpp
    src/daemon.rc
)
target_include_directories(jump_mouse_daemon PRIVATE src)
target_link_libraries(jump_mouse_daemon PRIVATE user32 kernel32 shell32)

# ========== Config tool ==========
add_executable(jump_mouse_config WIN32
    src/config_tool/config_main.cpp
    src/config_tool/config_dialog.cpp
    src/config_tool/daemon_controller.cpp
    src/common/config_manager.cpp
    src/common/adaptation_layer.cpp
    src/mouse_focus.rc
)
target_include_directories(jump_mouse_config PRIVATE src)
target_link_libraries(jump_mouse_config PRIVATE user32 kernel32 shell32 comctl32)

# ========== Copy config and icon to output ==========
add_custom_command(TARGET jump_mouse_daemon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:jump_mouse_daemon>/config.json"
)
add_custom_command(TARGET jump_mouse_config POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:jump_mouse_config>/config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/ico图标/1.ico"
        "$<TARGET_FILE_DIR:jump_mouse_config>/1.ico"
)

# ========== DPI awareness ==========
# Embedded via .rc files (dpi_aware.manifest). Disable MSVC auto-manifest.
target_link_options(jump_mouse_daemon PRIVATE "/MANIFEST:NO")
target_link_options(jump_mouse_config PRIVATE "/MANIFEST:NO")

# ========== Release package: collect both exes + config ==========
add_custom_target(release ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/release"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:jump_mouse_daemon>"
        "${CMAKE_BINARY_DIR}/release/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:jump_mouse_config>"
        "${CMAKE_BINARY_DIR}/release/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        "${CMAKE_BINARY_DIR}/release/"
    DEPENDS jump_mouse_daemon jump_mouse_config
    COMMENT "Packaging release into build/release/"
)
