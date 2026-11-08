# 3D models (STL, OBJ, PLY, glTF, FBX, ...)
#
# The renderer is a small static library, like the CAD one: assimp parses the
# file into triangles and the software rasterizer paints them into a QImage.
#
# assimp is loaded at runtime (LoadLibrary) and only its headers are needed at
# build time: the library ships with the optional formats pack, and an
# installation without it must still start. Point ASSIMP_ROOT at a build of the
# headers (`D:/assimp-min/include`, see README); without them the module
# compiles to a stub that reports 3D formats unsupported.

add_library(model3d STATIC
    model3d.cpp
    model3d.h
)
target_link_libraries(model3d PUBLIC Qt${QT_VERSION_MAJOR}::Gui)
target_include_directories(model3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(ASSIMP_ROOT "" CACHE PATH "Prefix of a custom assimp build (headers only)")

find_path(ASSIMP_INCLUDE_DIR assimp/scene.h
    PATHS ${ASSIMP_ROOT}/include
    NO_DEFAULT_PATH)
if(NOT ASSIMP_INCLUDE_DIR)
    find_path(ASSIMP_INCLUDE_DIR assimp/scene.h)
endif()

if(ASSIMP_INCLUDE_DIR)
    target_include_directories(model3d PUBLIC ${ASSIMP_INCLUDE_DIR})
    target_compile_definitions(model3d PUBLIC USE_ASSIMP)
    message(STATUS "3D models: assimp headers from ${ASSIMP_INCLUDE_DIR}")
else()
    message(STATUS "3D models: assimp headers not found, 3D formats disabled")
endif()
