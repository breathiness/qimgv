# Paged pdf view
#
# The model and the widget are a small library the application links.

add_library(pdfpages STATIC
    pdfpagemodel.cpp
    pdfpagemodel.h
    pdfview.cpp
    pdfview.h
)
target_link_libraries(pdfpages PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
)
# pdfprovider.cpp lives in qimgv/ and talks to the plugin through QLibrary
target_sources(pdfpages PRIVATE ${CMAKE_SOURCE_DIR}/qimgv/utils/pdfprovider.cpp)
target_include_directories(pdfpages PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/qimgv
    ${CMAKE_SOURCE_DIR}
)
set_target_properties(pdfpages PROPERTIES AUTOMOC ON)

