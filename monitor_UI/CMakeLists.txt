# 仅在 Windows 或明确要求时编译
if(NOT WIN32 AND NOT MINGW AND NOT MSVC)
    message(STATUS "Skipping monitor_UI: not on Windows")
    return()
endif()

cmake_minimum_required(VERSION 3.19)
project(monitor_UI LANGUAGES CXX)

# 一次 find_package 包含所有需要的组件
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Charts)

qt_standard_project_setup()

qt_add_executable(monitor_UI
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

# 一次 target_link_libraries 链接所有模块
target_link_libraries(monitor_UI
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Charts
)

install(TARGETS monitor_UI
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET monitor_UI
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})