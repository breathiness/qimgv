cmake_minimum_required(VERSION 3.11)
project(pdfprovider
    DESCRIPTION "PDF page renderer for qimgv (poppler)"
    LANGUAGES CXX)

# Optional: only built when poppler's Qt6 bindings are available. The result is a
# self-contained plugin that can be dropped next to the executable.
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_QT6 IMPORTED_TARGET poppler-qt6)

if(NOT POPPLER_QT6_FOUND)
    message(STATUS "pdfprovider: poppler-qt6 not found, PDF support will not be built")
    return()
endif()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

add_library(pdfprovider MODULE
    pdfprovider.cpp
    pdfprovider.h
)

target_link_libraries(pdfprovider PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    PkgConfig::POPPLER_QT6)

set_target_properties(pdfprovider PROPERTIES
    CXX_EXTENSIONS OFF
    PREFIX "")

# the app looks for <exe dir>/pdf/<plugin>, so the pack is a folder that is copied
# in as a whole
if(WIN32)
    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/pdf")
else()
    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/pdf")
endif()
set_target_properties(pdfprovider PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

if(WIN32)
    install(TARGETS pdfprovider LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/pdf")
else()
    install(TARGETS pdfprovider LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/pdf")
endif()
