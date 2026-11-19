cmake_minimum_required(VERSION 3.25)

project(JRTE.Desktop
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets)

add_executable(JRTE.Desktop
    src/main.cpp

    src/ui/MainWindow.h
    src/ui/MainWindow.cpp

    src/ui/VideoWidget.h
    src/ui/VideoWidget.cpp

    src/ui/DiagnosticsPanel.h
    src/ui/DiagnosticsPanel.cpp

    src/engine/NativeVideoEngine.h
    src/engine/NativeVideoEngine.cpp
)

target_link_libraries(JRTE.Desktop
    PRIVATE Qt6::Widgets
)

target_include_directories(JRTE.Desktop
    PRIVATE src
)

if(WIN32)
    set_target_properties(JRTE.Desktop PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
