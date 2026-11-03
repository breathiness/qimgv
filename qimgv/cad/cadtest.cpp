// Standalone check for the CAD renderer: renders a DXF/DWG into a PNG so the
// result can be compared with the same file opened in a CAD application.
//
//   cadtest <input.dxf|input.dwg> <output.png> [maxSize]

#include <QGuiApplication>
#include <QImage>
#include <QString>
#include <QStringList>
#include <cstdio>

#include "caddrawing.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments();
    if(args.size() < 3) {
        std::fprintf(stderr, "usage: %s <input.dxf|input.dwg> <output.png> [maxSize]\n", qPrintable(args.value(0)));
        return 2;
    }
    const QString input = args.at(1);
    const QString output = args.at(2);
    const int maxSize = args.size() > 3 ? args.at(3).toInt() : 2000;

    CadDrawing drawing;
    if(!drawing.load(input)) {
        std::fprintf(stderr, "failed to load %s: %s\n", qPrintable(input), qPrintable(drawing.errorString()));
        return 1;
    }

    const QRectF bounds = drawing.bounds();
    double scale = 1.0;
    if(bounds.width() > 0 && bounds.height() > 0)
        scale = maxSize / std::max(bounds.width(), bounds.height());
    const int width = std::max(16, int(bounds.width() * scale));
    const int height = std::max(16, int(bounds.height() * scale));

    const QImage image = drawing.render(width, height);
    std::printf("%s: %d primitives, bounds %.2f x %.2f -> %dx%d\n",
                qPrintable(input), drawing.primitiveCount(),
                bounds.width(), bounds.height(), width, height);
    if(!image.save(output)) {
        std::fprintf(stderr, "could not write %s\n", qPrintable(output));
        return 1;
    }
    return 0;
}
