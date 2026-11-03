# libdxfrw: DXF/DWG reading (vendored)
#
# Upstream: https://github.com/LibreCAD/libdxfrw  (2.0.0, commit d68fb1e)
# Licence:  GPL-2.0-or-later, see COPYING. qimgv is GPLv3, so linking this is
#           compatible.
#
# Only the library is built here; upstream's dwg2dxf CLI and its tests are not
# part of this copy. The sources under src/ are untouched, so the tree can be
# refreshed by copying src/ from a newer upstream checkout.

set(LIBDXFRW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(LIBDXFRW_SOURCES
    ${LIBDXFRW_DIR}/intern/drw_dbg.cpp
    ${LIBDXFRW_DIR}/intern/drw_textcodec.cpp
    ${LIBDXFRW_DIR}/intern/dwgbuffer.cpp
    ${LIBDXFRW_DIR}/intern/dwgobjectframe.cpp
    ${LIBDXFRW_DIR}/intern/dwgbufferw.cpp
    ${LIBDXFRW_DIR}/intern/dwgwriter15.cpp
    ${LIBDXFRW_DIR}/intern/dwgwriter18.cpp
    ${LIBDXFRW_DIR}/intern/dwgwriter21.cpp
    ${LIBDXFRW_DIR}/intern/dwgwriter24.cpp
    ${LIBDXFRW_DIR}/intern/dwgwriter27.cpp
    ${LIBDXFRW_DIR}/intern/dwgwriter32.cpp
    ${LIBDXFRW_DIR}/intern/dwgreader.cpp
    ${LIBDXFRW_DIR}/intern/dwgreaderR1_40.cpp
    ${LIBDXFRW_DIR}/intern/dwgreaderR11.cpp
    ${LIBDXFRW_DIR}/intern/dwgreader15.cpp
    ${LIBDXFRW_DIR}/intern/dwgreader18.cpp
    ${LIBDXFRW_DIR}/intern/dwgreader21.cpp
    ${LIBDXFRW_DIR}/intern/dwgreader24.cpp
    ${LIBDXFRW_DIR}/intern/dwgreader27.cpp
    ${LIBDXFRW_DIR}/intern/dwgreader32.cpp
    ${LIBDXFRW_DIR}/intern/dwgutil.cpp
    ${LIBDXFRW_DIR}/intern/proxygraphicdecoder.cpp
    ${LIBDXFRW_DIR}/intern/dxfreader.cpp
    ${LIBDXFRW_DIR}/intern/dxfwriter.cpp
    ${LIBDXFRW_DIR}/intern/dwg_dxf_output_transaction.cpp
    ${LIBDXFRW_DIR}/intern/dwgwriterlayoutvalidation.cpp
    ${LIBDXFRW_DIR}/intern/rscodec.cpp
    ${LIBDXFRW_DIR}/drw_acis.cpp
    ${LIBDXFRW_DIR}/drw_base.cpp
    ${LIBDXFRW_DIR}/drw_classes.cpp
    ${LIBDXFRW_DIR}/drw_datastorage.cpp
    ${LIBDXFRW_DIR}/drw_entities.cpp
    ${LIBDXFRW_DIR}/drw_header.cpp
    ${LIBDXFRW_DIR}/drw_objects.cpp
    ${LIBDXFRW_DIR}/libdwgr.cpp
    ${LIBDXFRW_DIR}/libdxfrw.cpp
)

add_library(dxfrw STATIC ${LIBDXFRW_SOURCES})
target_include_directories(dxfrw PUBLIC ${LIBDXFRW_DIR} ${LIBDXFRW_DIR}/intern)
target_compile_features(dxfrw PUBLIC cxx_std_17)
# the vendored code has its own warning habits; keep them out of qimgv's build
target_compile_options(dxfrw PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-w>
    $<$<CXX_COMPILER_ID:Clang>:-w>)
