// PDF provider plugin: renders PDF pages for qimgv through a C interface.
//
// Poppler does the parsing and the vector->bitmap work; this file only maps the
// small C ABI in pdfprovider.h onto it.

#include "pdfprovider.h"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <cstring>
#include <memory>

#include <poppler-qt6.h>

struct qimgv_pdf {
    Poppler::Document *document = nullptr;
};

namespace {

// Poppler returns BGR(A) layouts on some builds; normalise to RGBA8888
void copyToRgba(const QImage &image, unsigned char *out, int width, int height) {
    const QImage converted = image.convertToFormat(QImage::Format_RGBA8888);
    const int bytesPerLine = converted.bytesPerLine();
    const int rowBytes = width * 4;
    for(int y = 0; y < height; y++) {
        std::memcpy(out + size_t(y) * size_t(rowBytes),
                    converted.constScanLine(y),
                    size_t(rowBytes > bytesPerLine ? bytesPerLine : rowBytes));
    }
}

}  // namespace

extern "C" {

int qimgv_pdf_abi_version(void) {
    return QIMGV_PDF_ABI_VERSION;
}

qimgv_pdf *qimgv_pdf_open(const char *utf8Path, const char **error) {
    if(error)
        *error = nullptr;
    if(!utf8Path) {
        if(error)
            *error = "no path";
        return nullptr;
    }
    std::unique_ptr<Poppler::Document> document = Poppler::Document::load(QString::fromUtf8(utf8Path));
    if(!document) {
        if(error)
            *error = "not a pdf, or unreadable";
        return nullptr;
    }
    document->setRenderHint(Poppler::Document::TextAntialiasing, true);
    document->setRenderHint(Poppler::Document::Antialiasing, true);
    auto *handle = new qimgv_pdf();
    handle->document = document.release();
    return handle;
}

void qimgv_pdf_close(qimgv_pdf *document) {
    if(!document)
        return;
    delete document->document;
    delete document;
}

int qimgv_pdf_page_count(qimgv_pdf *document) {
    if(!document || !document->document)
        return 0;
    // an encrypted document reports 0 pages until it is unlocked
    return document->document->numPages();
}

int qimgv_pdf_page_size(qimgv_pdf *document, int page, double *widthPt, double *heightPt) {
    if(!document || !document->document)
        return 1;
    std::unique_ptr<Poppler::Page> pdfPage(document->document->page(page));
    if(!pdfPage)
        return 2;
    const QSizeF size = pdfPage->pageSizeF();
    if(widthPt)
        *widthPt = size.width();
    if(heightPt)
        *heightPt = size.height();
    return 0;
}

int qimgv_pdf_is_encrypted(qimgv_pdf *document) {
    if(!document || !document->document)
        return 0;
    return document->document->isEncrypted() ? 1 : 0;
}

int qimgv_pdf_unlock(qimgv_pdf *document, const char *utf8Password) {
    if(!document || !document->document)
        return 1;
    const QByteArray password(utf8Password ? utf8Password : "");
    // Poppler wants both; the user password is what a document asks for
    return document->document->unlock(password, password) ? 0 : 1;
}

int qimgv_pdf_render(qimgv_pdf *document, int page, int width, int height, unsigned char *rgba) {
    if(!document || !document->document || !rgba || width <= 0 || height <= 0)
        return 1;
    std::unique_ptr<Poppler::Page> pdfPage(document->document->page(page));
    if(!pdfPage)
        return 2;
    const QSizeF size = pdfPage->pageSizeF();
    if(size.width() <= 0 || size.height() <= 0)
        return 3;
    // resolution that makes the page exactly width x height pixels
    const double xres = 72.0 * double(width) / size.width();
    const double yres = 72.0 * double(height) / size.height();
    const QImage rendered = pdfPage->renderToImage(xres, yres);
    if(rendered.isNull())
        return 4;
    copyToRgba(rendered, rgba, width, height);
    return 0;
}

}  // extern "C"
