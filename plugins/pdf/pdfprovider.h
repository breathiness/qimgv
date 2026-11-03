#pragma once

// C interface between qimgv and the optional PDF provider plugin.
//
// The plugin links against Poppler, which is GPL-2.0-or-later; qimgv is GPLv3, so
// the two can be combined. Keeping the boundary C-only means the app never sees
// Poppler's C++ ABI and the plugin can be rebuilt without touching the app.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QIMGV_PDF_ABI_VERSION 1

#ifdef _WIN32
#    define QIMGV_PDF_EXPORT __declspec(dllexport)
#else
#    define QIMGV_PDF_EXPORT __attribute__((visibility("default")))
#endif

typedef struct qimgv_pdf qimgv_pdf;

// ABI check: the app refuses a plugin that reports a different number.
QIMGV_PDF_EXPORT int qimgv_pdf_abi_version(void);

// Opens a file. Returns NULL on failure and, when error is not NULL, points it at
// a static string describing why. The document may still need a password; see
// qimgv_pdf_is_encrypted()/qimgv_pdf_unlock().
QIMGV_PDF_EXPORT qimgv_pdf *qimgv_pdf_open(const char *utf8Path, const char **error);
QIMGV_PDF_EXPORT void qimgv_pdf_close(qimgv_pdf *document);

QIMGV_PDF_EXPORT int qimgv_pdf_page_count(qimgv_pdf *document);
// page numbers are 0-based; sizes are in points (72 per inch)
QIMGV_PDF_EXPORT int qimgv_pdf_page_size(qimgv_pdf *document, int page,
                                         double *widthPt, double *heightPt);
QIMGV_PDF_EXPORT int qimgv_pdf_is_encrypted(qimgv_pdf *document);
// 0 on success, non-zero when the password is wrong
QIMGV_PDF_EXPORT int qimgv_pdf_unlock(qimgv_pdf *document, const char *utf8Password);

// Renders one page into an RGBA8888 buffer of width*height*4 bytes. Returns 0 on
// success. The page is scaled to fill the given size, keeping its aspect ratio is
// the caller's business (pass the size that matches the page's own ratio).
QIMGV_PDF_EXPORT int qimgv_pdf_render(qimgv_pdf *document, int page,
                                      int width, int height, unsigned char *rgba);

#ifdef __cplusplus
}
#endif
