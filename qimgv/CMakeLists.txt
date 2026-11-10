include(GNUInstallDirs)

set(CMAKE_VERBOSE_MAKEFILE ON)

if(APPLE)
    set(APPLE_ICON_NAME "qimgv.icns")
    set(APPLE_ICON_PATH "distrib/${APPLE_ICON_NAME}")
endif()

# VERSION METADATA
# The project() version in the top-level CMakeLists.txt is the single source of
# truth: it becomes the compiler-visible QIMGV_VERSION_* macros and, on Windows,
# the executable's VERSIONINFO resource (Explorer "Details" tab).
configure_file(qimgv-version.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/qimgv-version.h" @ONLY)
configure_file(qimgv-version.rc.in
    "${CMAKE_CURRENT_BINARY_DIR}/qimgv-version.rc" @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# TRANSLATIONS
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS LinguistTools)
# Example of creating a new translation file.
# 1. cd into qimgv directory and run:
#   lupdate -recursive . -ts res/translations/qimgv_en_US.ts    [replace "en_US" with your locale]
# 2. Add your .ts filename into TS_FILES list below
# 3. Translate it with Qt Linguist
set(TS_FILES
    "zh_CN.ts"
    "uk_UA.ts"
    "es_ES.ts"
    "de_DE.ts"
    "fr_FR.ts"
    "tr_TR.ts"
    "ja_JP.ts"
)
list(TRANSFORM TS_FILES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/res/translations/)
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION ${PROJECT_BINARY_DIR}/qimgv/translations)

# 1. Create 'qimgv_lupdate' target to update .ts files (needs to be called manually)
# 2. Compile .ts > .qm
if(${QT_VERSION_MAJOR} EQUAL 5)
    add_custom_target(qimgv_lupdate COMMAND ${Qt5_LUPDATE_EXECUTABLE} -recursive
        ${CMAKE_CURRENT_SOURCE_DIR} -ts ${TS_FILES})
    qt5_add_translation(QM_FILES ${TS_FILES})
endif()

# ADD EXECUTABLE
add_executable(qimgv
    appversion.cpp
    core.cpp
    main.cpp
    settings.cpp
    themestore.cpp
    sharedresources.cpp
    shortcutbuilder.cpp
    proxystyle.cpp
    uistyle.cpp
    uitheme.cpp
    materialstyle.cpp
    gui/customwidgets/materialdialog.cpp
    macosapplication.cpp
    resources.qrc
    fileassociations.cpp
    qimgv.rc
    "${CMAKE_CURRENT_BINARY_DIR}/qimgv-version.rc"
    ${APPLE_ICON_PATH}
    ${QM_FILES}
)

# The resource file names the icon the executable carries, but the generate step
# only looks at the .rc itself: without this the exe keeps an old icon after the
# .ico is replaced.
set_property(SOURCE qimgv.rc PROPERTY OBJECT_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/common/logo/app/qimgv.ico")

if(WIN32)
    # Per-format icons (ids 2001..2042) are pulled in by qimgv.rc itself: two
    # resource scripts would each number their RT_ICON entries from 1 and the
    # duplicates collide at link time, which makes DefaultIcon = "<exe>",-<id>
    # resolve to the wrong image. Keep them as dependencies so a changed .ico
    # relinks the executable.
    file(GLOB FORMAT_ICONS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/formats/*.ico")
    set_property(SOURCE qimgv.rc PROPERTY OBJECT_DEPENDS
                 "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/common/logo/app/qimgv.ico;${FORMAT_ICONS}")
    target_link_libraries(qimgv PRIVATE shell32 advapi32)
endif()

# Same idea for the version block: reconfigure whenever the project version or
# the template changes, so a bump always reaches the executable.
set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/qimgv-version.rc" PROPERTY OBJECT_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/qimgv-version.rc.in")
set_property(SOURCE appversion.cpp PROPERTY OBJECT_DEPENDS
             "${CMAKE_CURRENT_BINARY_DIR}/qimgv-version.h")

# update .ts, compile .qm (qt6 case)
if(${QT_VERSION_MAJOR} EQUAL 6)
    qt_add_lupdate(qimgv TS_FILES ${TS_FILES})
    qt_add_lrelease(qimgv TS_FILES ${TS_FILES})
endif()

# qtbase's own catalog (standard dialog buttons, platform theme strings) is not
# generated from TS_FILES; the checked-in .qm files go next to ours so the
# deployment step picks them up with the rest of translations/.
file(GLOB QT_BASE_TRANSLATIONS "${CMAKE_CURRENT_SOURCE_DIR}/res/translations/qtbase_*.qm")
if(QT_BASE_TRANSLATIONS)
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/qimgv/translations")
    file(COPY ${QT_BASE_TRANSLATIONS} DESTINATION "${PROJECT_BINARY_DIR}/qimgv/translations")
endif()

# ADD SOURCES
add_subdirectory(components)
add_subdirectory(sourcecontainers)
add_subdirectory(gui)
add_subdirectory(utils)
if(OPENCV_SUPPORT)
    add_subdirectory(3rdparty/QtOpenCV)
endif()

# C++ STANDARD
target_compile_features(qimgv PRIVATE cxx_std_17)
set_target_properties(qimgv PROPERTIES CXX_EXTENSIONS OFF)

# LINK STUFF
target_link_libraries(qimgv PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg Qt${QT_VERSION_MAJOR}::PrintSupport)
# CAD drawings: DXF/DWG rendering
target_link_libraries(qimgv PRIVATE cadrenderer pdfpages)
# 3D models: rendered into an image by the model3d library
target_link_libraries(qimgv PRIVATE model3d)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_link_libraries(qimgv PRIVATE Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
endif()

# OPTION DEFINITIONS, LINKING
if(QLEMENTINE AND TARGET qlementine)
    target_link_libraries(qimgv PRIVATE qlementine)
    target_compile_definitions(qimgv PRIVATE USE_QLEMENTINE)
    if(TARGET qlementine-icons)
        target_link_libraries(qimgv PRIVATE qlementine-icons)
    endif()
endif()
if(EXIV2)
    target_link_libraries(qimgv PRIVATE PkgConfig::Exiv2)
    target_compile_definitions(qimgv PRIVATE USE_EXIV2)
elseif(LibExif_FOUND)
    target_link_libraries(qimgv PRIVATE PkgConfig::LibExif)
    target_compile_definitions(qimgv PRIVATE USE_LIBEXIF)
endif()
if(KDE_SUPPORT)
    target_link_libraries(qimgv PRIVATE KF6::WindowSystem)
    target_compile_definitions(qimgv PRIVATE USE_KDE_BLUR)
endif()
if(VIDEO_SUPPORT)
    # Video support (player and thumbnail frames) ships with the optional
    # plugin pack: the executable only defines the feature flag and finds the
    # plugin at runtime, so an installation without the pack still starts.
    target_compile_definitions(qimgv PRIVATE USE_MPV)
    if(WIN32)
        ADD_DEFINITIONS(-D_QIMGV_PLUGIN_DIR="plugins")
        ADD_DEFINITIONS(-D_QIMGV_PLAYER_PLUGIN="player_mpv.dll")
    else()
        ADD_DEFINITIONS(-D_QIMGV_PLUGIN_DIR="${QIMGV_PLUGIN_DIR}")
        ADD_DEFINITIONS(-D_QIMGV_PLAYER_PLUGIN="player_mpv.so")
    endif()
endif()
if(OPENCV_SUPPORT)
    target_link_libraries(qimgv PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(qimgv PRIVATE USE_OPENCV)
endif()

# generate proper GUI program on specified platform
if(WIN32) # Check if we are on Windows
    if(MSVC) # Check if we are using the Visual Studio compiler
        set_target_properties(qimgv PROPERTIES WIN32_EXECUTABLE ON)
        target_link_options(qimgv PRIVATE "/ENTRY:mainCRTStartup")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(qimgv PRIVATE "-mwindows")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set(CMAKE_CXX_FLAGS "$ENV{CMAKE_CXX_FLAGS} -Wno-shift-negative-value")
        set(CMAKE_EXE_LINKER_FLAGS "$ENV{CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
        target_link_options(qimgv PRIVATE "-mwindows")
    else()
        message(SEND_ERROR "You are using an unsupported Windows compiler! (Not MSVC or GCC)")
    endif(MSVC)
elseif(APPLE)
    set_target_properties(qimgv PROPERTIES
        MACOSX_BUNDLE YES
        MACOSX_BUNDLE_BUNDLE_NAME "qimgv"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/distrib/MyBundleInfo.plist.in
        MACOSX_BUNDLE_ICON_FILE ${APPLE_ICON_NAME}
    )
    set_source_files_properties(${APPLE_ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
elseif(NOT UNIX)
    message(SEND_ERROR "You are on an unsupported platform! (Not Win32, Mac OS X or Unix)")
endif()

## INSTALLATION
install(TARGETS qimgv DESTINATION bin)

# .desktop
install(FILES distrib/qimgv.desktop
    DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/applications)

# icons
foreach(icon IN ITEMS 16x16 22x22 24x24 32x32 36x36 48x48 64x64 128x128 256x256)
    install(FILES distrib/hicolor/${icon}/apps/qimgv.png DESTINATION
        ${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/${icon}/apps)
endforeach()
install(FILES distrib/hicolor/scalable/apps/qimgv.svg
    DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/scalable/apps)

# AppData manifest
install(FILES distrib/qimgv.appdata.xml DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/metainfo)

# Install internationalization language files
if(WIN32)
    install(DIRECTORY "${PROJECT_BINARY_DIR}/qimgv/translations" DESTINATION "${CMAKE_INSTALL_BINDIR}")
else()
    ADD_DEFINITIONS(-DTRANSLATIONS_PATH="${CMAKE_INSTALL_FULL_DATAROOTDIR}/qimgv/translations")
    install(DIRECTORY "${PROJECT_BINARY_DIR}/qimgv/translations" DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/qimgv")
endif()

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()
