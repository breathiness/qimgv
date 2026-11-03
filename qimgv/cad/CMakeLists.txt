# CAD drawings (DXF/DWG)
#
# The renderer is a small static library so both qimgv and the standalone test
# below can use it. The test target is not installed.

add_library(cadrenderer STATIC
    caddrawing.cpp
    caddrawing.h
)
target_link_libraries(cadrenderer PUBLIC dxfrw Qt${QT_VERSION_MAJOR}::Gui)
target_include_directories(cadrenderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cadtest cadtest.cpp)
target_link_libraries(cadtest PRIVATE cadrenderer)
