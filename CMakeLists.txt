cmake_minimum_required(VERSION 3.20)
project(mouse_focus VERSION 1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: treat source files as UTF-8
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# ========== Daemon ==========
add_executable(mouse_focus_daemon WIN32
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
target_include_directories(mouse_focus_daemon PRIVATE src)
target_link_libraries(mouse_focus_daemon PRIVATE user32 kernel32 shell32)

# ========== Config tool ==========
add_executable(mouse_focus_config WIN32
    src/config_tool/config_main.cpp
    src/config_tool/config_dialog.cpp
    src/config_tool/daemon_controller.cpp
    src/common/config_manager.cpp
    src/common/adaptation_layer.cpp
    src/daemon/mouse_controller.cpp
    src/mouse_focus.rc
)
target_include_directories(mouse_focus_config PRIVATE src)
target_link_libraries(mouse_focus_config PRIVATE user32 kernel32 shell32 comctl32)

# ========== Test harness (console) ==========
add_executable(mouse_focus_test
    src/test/test_harness.cpp
    src/common/config_manager.cpp
    src/common/adaptation_layer.cpp
    src/daemon/mouse_controller.cpp
    src/config_tool/daemon_controller.cpp
)
target_include_directories(mouse_focus_test PRIVATE src)
target_link_libraries(mouse_focus_test PRIVATE user32 kernel32 shell32)

# ========== Copy config and icon to output ==========
add_custom_command(TARGET mouse_focus_daemon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:mouse_focus_daemon>/config.json"
)
add_custom_command(TARGET mouse_focus_config POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:mouse_focus_config>/config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/ico图标/1.ico"
        "$<TARGET_FILE_DIR:mouse_focus_config>/1.ico"
)

# ========== DPI awareness ==========
# Embedded via .rc files (dpi_aware.manifest). Disable MSVC auto-manifest.
target_link_options(mouse_focus_daemon PRIVATE "/MANIFEST:NO")
target_link_options(mouse_focus_config PRIVATE "/MANIFEST:NO")
