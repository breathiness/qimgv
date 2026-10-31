cmake_minimum_required(VERSION 3.11)
project(qapng
    DESCRIPTION "animated PNG reader for qimgv"
    LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

# Qt image format plugin: deployed next to qimgv in imageformats/
add_library(qapng MODULE
    apnghandler.cpp
    apnghandler.h
    main.cpp
)

target_link_libraries(qapng PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
target_compile_features(qapng PRIVATE cxx_std_11)
set_target_properties(qapng PROPERTIES
    CXX_EXTENSIONS OFF
    PREFIX ""
    OUTPUT_NAME "qapng"
)
