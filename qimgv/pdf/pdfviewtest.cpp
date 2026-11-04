// Standalone check for the paged pdf view: opens a document, renders a few pages
// and saves what the widget would show. Used while the view is being built, before
// it is wired into the application.
//
//   pdfviewtest <file.pdf> [page] [out.png] [width] [height]

#include "pdfpagemodel.h"
#include "pdfview.h"

#include "settings.h"

#include <QApplication>
#include <QImage>
#include <QTextStream>
#include <QTimer>

// The view asks the application's settings for the window background. This tool is
// not the application, so the pointer stays null and the view falls back to its
// palette.
Settings *settings = nullptr;
static ColorScheme testColorScheme;
ColorScheme::ColorScheme() : tid(0) {}
const ColorScheme &Settings::colorScheme() { return testColorScheme; }

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTextStream out(stdout);

    const QStringList args = QCoreApplication::arguments();
    if(args.size() < 2) {
        out << "usage: pdfviewtest <file.pdf> [page] [out.png] [width] [height]\n";
        return 2;
    }
    const QString path = args.at(1);
    const int page = args.size() > 2 ? args.at(2).toInt() : 0;
    const QString outPath = args.size() > 3 ? args.at(3) : QStringLiteral("pdfviewtest.png");
    const int width = args.size() > 4 ? args.at(4).toInt() : 1000;
    const int height = args.size() > 5 ? args.at(5).toInt() : 800;

    auto model = std::make_shared<PdfPageModel>();
    QString error;
    if(!model->open(path, &error)) {
        out << "open failed: " << error << "\n";
        return 1;
    }
    out << "pages: " << model->pageCount()
        << "  max page: " << model->maxPageSize().width() << "x" << model->maxPageSize().height()
        << "  encrypted: " << model->isEncrypted() << "\n";

    PdfView view;
    view.resize(width, height);
    view.setModel(model);
    view.show();
    view.goToPage(page);

    // give the worker time to render the visible pages, then save what is on screen
    QTimer::singleShot(2500, [&]() {
        const QImage shot = view.grab().toImage();
        const bool ok = shot.save(outPath);
        out << "page " << view.currentPage() + 1 << "/" << view.pageCount()
            << "  zoom " << view.zoom()
            << "  saved: " << ok << " " << outPath << "\n";
        app.quit();
    });
    return app.exec();
}
