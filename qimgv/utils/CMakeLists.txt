## SOURCES FOR QIMGV UTILITY CLASSES

target_sources(qimgv PRIVATE
    actions.cpp
    cmdoptionsrunner.cpp
    imagefactory.cpp
    imagelib.cpp
    imageinfo.cpp
    exifreader.cpp
    inputmap.cpp
    randomizer.cpp
    script.cpp
    sleep.cpp
    stuff.cpp
    wallpapersetter.cpp
    fileoperations.cpp
    pdfprovider.cpp
    pdfprovider.h
    ziparchive.cpp
    ziparchive.h
)

# comic archives (CBZ) are zips
find_package(ZLIB REQUIRED)
target_include_directories(qimgv PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(qimgv PRIVATE ${ZLIB_LIBRARIES})
target_compile_definitions(qimgv PRIVATE USE_ZLIB)
