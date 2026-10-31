# qimgv root
cmake_minimum_required(VERSION 3.13)
project(qimgv
    VERSION 1.0.2
    HOMEPAGE_URL "https://github.com/easymodo/qimgv"
    LANGUAGES CXX)

message(STATUS "Build configuration: " ${CMAKE_BUILD_TYPE})

# check for gcc
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
        CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    message(FATAL_ERROR "\n!!! THIS PROJECT REQUIRES GCC 9.0 OR LATER !!!\n")
endif()

# AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# USER OPTIONS
# Usage: cmake -DVIDEO_SUPPORT=ON [...]
option(EXIV2 "For reading/writing exif tags" ON)
option(VIDEO_SUPPORT "Enable video support" ON)
option(OPENCV_SUPPORT "Enable HQ scaling via OpenCV" ON)
option(KDE_SUPPORT "Support blur when using KDE" OFF)
option(USE_QT5 "Build with Qt5 if available" ON)
option(QLEMENTINE "Use the Qlementine widget style (requires Qt6)" OFF)

if(UNIX AND NOT APPLE)
    set(QT_EXTERN_PATH "" CACHE STRING "Tell compile external QT path, example: (/opt/Qt/6.2.0/gcc_64)")
    string(COMPARE EQUAL "${QT_EXTERN_PATH}" "" result)
    if (NOT result)
        set(CMAKE_INSTALL_RPATH "${QT_EXTERN_PATH}/lib")
    endif()
endif()

# FIND PACKAGES
if(USE_QT5) # Try Qt5 first if USE_QT5 is set
    find_package(Qt5 COMPONENTS Widgets QUIET)
    if(Qt5_FOUND)
        find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core Widgets Svg PrintSupport)
    else()
        find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Svg PrintSupport OpenGLWidgets)
    endif()
else() # Try Qt6 otherwise
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Svg PrintSupport OpenGLWidgets)
    else()
        find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core Widgets Svg PrintSupport)
    endif()
endif()

message(STATUS "Qt Version: " ${QT_VERSION})
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Svg PrintSupport OpenGLWidgets)
else()
    find_package(Qt${QT_VERSION_MAJOR} 5.12 REQUIRED COMPONENTS Core Widgets Svg PrintSupport)
endif()

if(EXIV2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Exiv2 REQUIRED IMPORTED_TARGET exiv2)
endif()

if(KDE_SUPPORT)
    find_package(KF6WindowSystem REQUIRED)
endif()

if(OPENCV_SUPPORT)
    find_package(PkgConfig REQUIRED)
    find_package(OpenCV REQUIRED core imgproc)
endif()

##############################################################

if(WIN32 AND VIDEO_SUPPORT)
    # libmpv for the Windows build lives in the source tree: include/mpv/,
    # lib/x86_64/libmpv.dll.a and the runtime dll in bin/. It is not tracked by
    # git (see .gitignore), so a clone has to drop a libmpv package in there;
    # -DMPV_DIR still overrides this for a system-wide install.
    if(NOT MPV_DIR)
        set(MPV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mpv" CACHE PATH
            "libmpv dev kit (include/ and lib/x86_64/libmpv.dll.a)")
    endif()
    if(NOT EXISTS "${MPV_DIR}/include/mpv/client.h")
        message(FATAL_ERROR
            "MPV_DIR (${MPV_DIR}) does not hold the libmpv dev kit. Put the "
            "headers in <dir>/include/mpv/ and the import library in "
            "<dir>/lib/x86_64/libmpv.dll.a, or configure with -DMPV_DIR=<path>.")
    endif()
endif()

if(QLEMENTINE)
    if(QT_VERSION_MAJOR LESS 6)
        message(FATAL_ERROR "QLEMENTINE requires Qt6 (configure with -DUSE_QT5=OFF)")
    endif()
    # both are submodules; a plain clone leaves the directories empty
    foreach(dep qlementine qlementine-icons)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/${dep}/CMakeLists.txt")
            message(FATAL_ERROR
                "3rdparty/${dep} is empty - run: git submodule update --init --recursive")
        endif()
    endforeach()
    add_subdirectory(3rdparty/qlementine EXCLUDE_FROM_ALL)
    add_subdirectory(3rdparty/qlementine-icons EXCLUDE_FROM_ALL)
endif()

add_subdirectory(qimgv)

if(VIDEO_SUPPORT)
    add_subdirectory(plugins/player_mpv)
endif()

# image format plugins: APNG, and the optional pack with HEIC/AVIF, JPEG XL,
# camera raw and PSD. Keeping them out of the executable means they can be
# distributed separately, and a missing pack only removes formats.
add_subdirectory(plugins/imageformats/apng)
add_subdirectory(plugins/imageformats/formats)
