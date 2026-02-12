cmake_minimum_required(VERSION 3.16)
project(TestUI)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# TestUI Library - Simple Hello World test
# SHARED library for dynamic loading (.dll on Windows)
add_library(TestUI SHARED
    # UI Factory (zero dependency)
    TestUIFactory.h
    TestUIFactory.cpp

    # UI Components
    TestMainWindow.h
    TestMainWindow.cpp
)

target_link_libraries(TestUI
    PUBLIC
    Qt6::Core
    Qt6::Widgets
    MusicPlayerCore
)

target_include_directories(TestUI
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    # Core library includes
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Public/Framework
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Public/Subsystems
)

# Export symbols for .dll
target_compile_definitions(TestUI PRIVATE UI_LIBRARY_EXPORT)

# Windows-specific: Auto-export all symbols (like MusicPlayerCore)
if(WIN32)
    set_target_properties(TestUI PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

set_target_properties(TestUI PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    PREFIX ""  # Don't add "lib" prefix on Windows MinGW
    OUTPUT_NAME "TestUI"
)
