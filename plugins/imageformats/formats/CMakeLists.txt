cmake_minimum_required(VERSION 3.11)
project(qimgvformats
    DESCRIPTION "optional image formats for qimgv (HEIC, JPEG XL, camera raw, PSD)"
    LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# The plugin metadata (the suffix list Qt and qimgv read) lives in formats.json
# and is compiled in by moc. Without this filter AUTOMOC does not notice a change
# to that file and the plugin keeps advertising the old suffixes.
set(CMAKE_AUTOMOC_DEPEND_FILTERS
    Q_PLUGIN_METADATA "FILE \"([^\"]+)\""
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

add_library(qimgvformats MODULE
    formatshandler.cpp
    formatshandler.h
    main.cpp
    decoders/anidecoder.cpp
    decoders/anidecoder.h
    decoders/archivedecoder.cpp
    decoders/archivedecoder.h
    decoders/exrdecoder.cpp
    decoders/exrdecoder.h
    decoders/hdrdecoder.cpp
    decoders/hdrdecoder.h
    decoders/heifdecoder.cpp
    decoders/heifdecoder.h
    decoders/jxldecoder.cpp
    decoders/jxldecoder.h
    decoders/psddecoder.cpp
    decoders/psddecoder.h
    decoders/rawdecoder.cpp
    decoders/rawdecoder.h
)

target_link_libraries(qimgvformats PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
# OpenEXR 3 headers need C++17
target_compile_features(qimgvformats PRIVATE cxx_std_17)
target_include_directories(qimgvformats PRIVATE decoders)

# each codec is optional at build time; the handler only offers what it has
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LibHeif IMPORTED_TARGET libheif)
    pkg_check_modules(LibJxl IMPORTED_TARGET libjxl)
    pkg_check_modules(LibJxlThreads IMPORTED_TARGET libjxl_threads)
    pkg_check_modules(LibRaw IMPORTED_TARGET libraw_r)
endif()

if(LibHeif_FOUND)
    target_link_libraries(qimgvformats PRIVATE PkgConfig::LibHeif)
    target_compile_definitions(qimgvformats PRIVATE USE_LIBHEIF)
endif()
if(LibJxl_FOUND)
    target_link_libraries(qimgvformats PRIVATE PkgConfig::LibJxl)
    target_compile_definitions(qimgvformats PRIVATE USE_LIBJXL)
    if(LibJxlThreads_FOUND)
        target_link_libraries(qimgvformats PRIVATE PkgConfig::LibJxlThreads)
    endif()
endif()
if(LibRaw_FOUND)
    target_link_libraries(qimgvformats PRIVATE PkgConfig::LibRaw)
    target_compile_definitions(qimgvformats PRIVATE USE_LIBRAW)
endif()

# OpenEXR (HDR rendering images); the decoder is only compiled in when the
# library is around, like every other optional codec here
find_package(OpenEXR QUIET)
if(OpenEXR_FOUND)
    target_link_libraries(qimgvformats PRIVATE OpenEXR::OpenEXR)
    target_compile_definitions(qimgvformats PRIVATE USE_OPENEXR)
endif()

# PSD files may store their image data zlib compressed
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(qimgvformats PRIVATE ZLIB::ZLIB)
    target_compile_definitions(qimgvformats PRIVATE USE_ZLIB)
endif()

set_target_properties(qimgvformats PROPERTIES
    CXX_EXTENSIONS OFF
    PREFIX ""
    OUTPUT_NAME "qimgvformats"
)
