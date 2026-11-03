#include "caddrawing.h"

#include <QFont>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QThread>
#include <QTransform>
#include <algorithm>
#include <cmath>

#include "libdxfrw.h"
#include "libdwgr.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
// how many segments a full circle gets at most; enough for a 4k-wide view
constexpr int kMaxCircleSegments = 720;
// hard limit on how many entities an INSERT chain may expand into, so a broken
// drawing cannot make the loader run forever
constexpr int kMaxInstances = 200000;
constexpr int kMaxInsertDepth = 16;

// AutoCAD's first seven colours; everything above them is mapped to the nearest of
// these, which is good enough for reading a drawing
// ACI colours are meant for a black model space; a viewer shows the drawing on
// paper, so the bright ones are darkened until they read on white.
QColor forLightBackground(const QColor &color) {
    if(color.lightnessF() < 0.62)
        return color;
    return QColor::fromHsvF(color.hsvHueF(), color.saturationF(), color.valueF() * 0.62);
}

QColor aciColor(int index) {
    switch(index) {
        case 1: return forLightBackground(QColor(255, 0, 0));
        case 2: return forLightBackground(QColor(255, 255, 0));
        case 3: return forLightBackground(QColor(0, 255, 0));
        case 4: return forLightBackground(QColor(0, 255, 255));
        case 5: return forLightBackground(QColor(0, 0, 255));
        case 6: return forLightBackground(QColor(255, 0, 255));
        case 7: return QColor(0, 0, 0);
        default: break;
    }
    if(index >= 250 && index <= 255)
        return QColor(64, 64, 64);
    // 8..249: greys and colour wheel entries - keep them grey so the drawing stays
    // readable on white
    return QColor(96, 96, 96);
}

QPointF toPoint(const DRW_Coord &coord) {
    return QPointF(coord.x, coord.y);
}

// one arc segment of a polyline bulge (code 42): bulge = tan(includedAngle / 4)
void appendBulge(QPolygonF &points, const QPointF &from, const QPointF &to, double bulge) {
    if(std::abs(bulge) < 1e-9) {
        points << to;
        return;
    }
    const QPointF chord = to - from;
    const double chordLength = std::hypot(chord.x(), chord.y());
    if(chordLength < 1e-12) {
        points << to;
        return;
    }
    const double theta = 4.0 * std::atan(bulge);          // included angle
    const double radius = chordLength / (2.0 * std::sin(std::abs(theta) / 2.0));
    // centre is on the perpendicular bisector, offset by the apothem
    const QPointF mid = (from + to) / 2.0;
    const QPointF normal(-chord.y() / chordLength, chord.x() / chordLength);
    const double apothem = radius * std::cos(theta / 2.0);
    const QPointF center = mid + normal * (bulge > 0 ? -apothem : apothem);
    const double startAngle = std::atan2(from.y() - center.y(), from.x() - center.x());
    const int segments = std::max(2, int(std::abs(theta) / (kPi / 24.0)) + 1);
    for(int i = 1; i <= segments; i++) {
        const double a = startAngle + theta * i / segments;
        points << center + QPointF(std::cos(a), std::sin(a)) * radius;
    }
}

}  // namespace

bool CadDrawing::load(const QString &filePath) {
    mLoaded = false;
    mError.clear();
    mBounds = QRectF();
    mHasBounds = false;
    mPrimitives.clear();
    mBlocks.clear();
    mBlockBasePoints.clear();
    mLayerColors.clear();
    mCurrentBlock.clear();
    mCurrentTransform = QTransform();

    const QFileInfo info(filePath);
    if(!info.exists()) {
        mError = QStringLiteral("file not found");
        return false;
    }
    const QString suffix = info.suffix().toLower();
    bool ok = false;
    if(suffix == QStringLiteral("dwg")) {
        dwgR reader(filePath.toStdString().c_str());
        ok = reader.read(this, /*ext=*/true);
        if(!ok)
            mError = QStringLiteral("DWG read error %1").arg(int(reader.getError()));
    } else {
        dxfRW reader(filePath.toStdString().c_str());
        ok = reader.read(this, /*ext=*/true);
        if(!ok)
            mError = QStringLiteral("DXF read error %1").arg(int(reader.getError()));
    }
    if(!ok)
        return false;
    computeBounds();
    mLoaded = !mPrimitives.empty();
    if(!mLoaded)
        mError = QStringLiteral("no drawable entities");
    return mLoaded;
}

void CadDrawing::addLayer(const DRW_Layer &data) {
    const int index = data.color;
    const QColor color = (index > 0 && index < 256) ? aciColor(index) : QColor(Qt::black);
    mLayerColors.insert(QString::fromStdString(data.name), color);
}

QColor CadDrawing::resolveColor(const DRW_Entity &entity, QColor fallback) const {
    // 256 = BYLAYER, 0 = BYBLOCK
    if(entity.color != 0 && entity.color < 256)
        return aciColor(entity.color);
    const auto it = mLayerColors.constFind(QString::fromStdString(entity.layer));
    if(it != mLayerColors.constEnd())
        return it.value();
    return fallback;
}

void CadDrawing::addBlock(const DRW_Block &data) {
    mCurrentBlock = QString::fromStdString(data.name);
    mBlocks[mCurrentBlock] = {};
    mBlockBasePoints[mCurrentBlock] = QPointF(data.basePoint.x, data.basePoint.y);
    mCurrentTransform = QTransform();
}

void CadDrawing::endBlock() {
    mCurrentBlock.clear();
    mCurrentTransform = QTransform();
}

// Bounding box of everything that was read, like a CAD program's "zoom extents".
// Values a file sometimes carries as sentinels (1e20 and friends) are dropped, and
// so are non-finite coordinates; anything else is taken literally so a drawing
// sheet's frame is not cropped.
void CadDrawing::computeBounds() {
    bool have = false;
    QRectF result;
    for(const Prim &prim : mPrimitives) {
        QPolygonF world;
        if(prim.kind == Prim::Kind::Poly || prim.kind == Prim::Kind::Solid) {
            world = prim.transform.map(prim.points);
        } else if(prim.kind == Prim::Kind::Curve) {
            world << prim.transform.map(QPointF(prim.center.x() - prim.radiusX, prim.center.y() - prim.radiusY))
                  << prim.transform.map(QPointF(prim.center.x() + prim.radiusX, prim.center.y() + prim.radiusY));
        } else if(prim.kind == Prim::Kind::Text) {
            // text is placed by its insertion point; the glyph box is estimated by
            // the caller when the primitive was created
            world << prim.transform.map(QPointF());
        } else {
            continue;
        }
        for(const QPointF &point : world) {
            if(!std::isfinite(point.x()) || !std::isfinite(point.y()))
                continue;
            if(std::abs(point.x()) > 1e9 || std::abs(point.y()) > 1e9)
                continue;
            if(!have) {
                result = QRectF(point, QSizeF(0, 0));
                have = true;
            } else {
                result.setLeft(std::min(result.left(), point.x()));
                result.setRight(std::max(result.right(), point.x()));
                result.setTop(std::min(result.top(), point.y()));
                result.setBottom(std::max(result.bottom(), point.y()));
            }
        }
    }
    if(!have)
        return;
    // a drawing of a single point would give a zero-sized box
    if(result.width() <= 0.0)
        result.setWidth(1.0);
    if(result.height() <= 0.0)
        result.setHeight(1.0);
    mBounds = result;
    mHasBounds = true;
}

void CadDrawing::addPrim(Prim prim) {
    if(mCurrentBlock.isEmpty()) {
        mPrimitives.push_back(std::move(prim));
        return;
    }
    // inside a block definition: keep it in block coordinates, the INSERT applies
    // its own transform later
    mBlocks[mCurrentBlock].push_back(std::move(prim));
}

void CadDrawing::accumulateBounds(const QPolygonF &worldPoints) {
    for(const QPointF &point : worldPoints) {
        if(!std::isfinite(point.x()) || !std::isfinite(point.y()))
            continue;
        if(!mHasBounds) {
            mBounds = QRectF(point, QSizeF(0, 0));
            mHasBounds = true;
        } else {
            const double minX = std::min(mBounds.left(), point.x());
            const double maxX = std::max(mBounds.right(), point.x());
            const double minY = std::min(mBounds.top(), point.y());
            const double maxY = std::max(mBounds.bottom(), point.y());
            mBounds = QRectF(minX, minY, maxX - minX, maxY - minY);
        }
    }
}

void CadDrawing::addPoint(const DRW_Point &data) {
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.points << toPoint(data.basePoint) << toPoint(data.basePoint) + QPointF(1e-6, 1e-6);
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    accumulateBounds(prim.transform.map(prim.points));
    addPrim(std::move(prim));
}

void CadDrawing::addLine(const DRW_Line &data) {
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.points << toPoint(data.basePoint) << toPoint(data.secPoint);
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    accumulateBounds(prim.transform.map(prim.points));
    addPrim(std::move(prim));
}

void CadDrawing::addRay(const DRW_Ray &data) {
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.points << toPoint(data.basePoint) << toPoint(data.secPoint);
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    accumulateBounds(prim.transform.map(prim.points));
    addPrim(std::move(prim));
}

void CadDrawing::addXline(const DRW_Xline &data) {
    addRay(DRW_Ray(data));
}

void CadDrawing::addCircle(const DRW_Circle &data) {
    Prim prim;
    prim.kind = Prim::Kind::Curve;
    prim.center = toPoint(data.basePoint);
    prim.radiusX = data.radious;
    prim.radiusY = data.radious;
    prim.startAngle = 0.0;
    prim.endAngle = 2.0 * kPi;
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    QPolygonF box;
    box << QPointF(prim.center.x() - prim.radiusX, prim.center.y() - prim.radiusY)
        << QPointF(prim.center.x() + prim.radiusX, prim.center.y() + prim.radiusY);
    accumulateBounds(prim.transform.map(box));
    addPrim(std::move(prim));
}

void CadDrawing::addArc(const DRW_Arc &data) {
    // the angle accessors are not const, so work on a copy
    DRW_Arc arc = data;
    Prim prim;
    prim.kind = Prim::Kind::Curve;
    prim.center = toPoint(data.basePoint);
    prim.radiusX = data.radious;
    prim.radiusY = data.radious;
    prim.startAngle = arc.startAngle();
    prim.endAngle = arc.endAngle();
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    QPolygonF box;
    box << QPointF(prim.center.x() - prim.radiusX, prim.center.y() - prim.radiusY)
        << QPointF(prim.center.x() + prim.radiusX, prim.center.y() + prim.radiusY);
    accumulateBounds(prim.transform.map(box));
    addPrim(std::move(prim));
}

void CadDrawing::addEllipse(const DRW_Ellipse &data) {
    // the ellipse is stored as a centre, the major axis end point and the ratio
    Prim prim;
    prim.kind = Prim::Kind::Curve;
    const QPointF center(data.basePoint.x, data.basePoint.y);
    const QPointF major(data.secPoint.x, data.secPoint.y);
    const double ratio = data.ratio > 0 ? data.ratio : 1.0;
    prim.center = center;
    prim.radiusX = std::hypot(major.x(), major.y());
    prim.radiusY = prim.radiusX * ratio;
    prim.startAngle = data.staparam;
    prim.endAngle = data.endparam;
    prim.color = resolveColor(data, Qt::black);
    // rotate the local ellipse so its major axis follows the entity
    const double rotation = std::atan2(major.y(), major.x()) * 180.0 / kPi;
    prim.transform = mCurrentTransform * QTransform().translate(center.x(), center.y())
                                                   .rotate(rotation)
                                                   .translate(-center.x(), -center.y());
    QPolygonF box;
    box << QPointF(center.x() - prim.radiusX, center.y() - prim.radiusY)
        << QPointF(center.x() + prim.radiusX, center.y() + prim.radiusY);
    accumulateBounds(prim.transform.map(box));
    addPrim(std::move(prim));
}

void CadDrawing::addLWPolyline(const DRW_LWPolyline &data) {
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    QPolygonF points;
    std::shared_ptr<DRW_Vertex2D> previous;
    for(const auto &vertex : data.vertlist) {
        if(!vertex)
            continue;
        const QPointF point(vertex->x, vertex->y);
        if(previous && std::abs(previous->bulge) > 1e-9) {
            appendBulge(points, QPointF(previous->x, previous->y), point, previous->bulge);
        } else {
            points << point;
        }
        previous = vertex;
    }
    if((data.flags & 1) && points.size() > 1)              // closed polyline
        points << points.first();
    if(points.isEmpty())
        return;
    prim.points = points;
    accumulateBounds(prim.transform.map(points));
    addPrim(std::move(prim));
}

void CadDrawing::addPolyline(const DRW_Polyline &data) {
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    QPolygonF points;
    for(const auto &vertex : data.vertlist) {
        if(!vertex)
            continue;
        points << QPointF(vertex->basePoint.x, vertex->basePoint.y);
    }
    if((data.flags & 1) && points.size() > 1)
        points << points.first();
    if(points.isEmpty())
        return;
    prim.points = points;
    accumulateBounds(prim.transform.map(points));
    addPrim(std::move(prim));
}

void CadDrawing::addSpline(const DRW_Spline *data) {
    if(!data)
        return;
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.color = resolveColor(*data, Qt::black);
    prim.transform = mCurrentTransform;
    QPolygonF points;
    // control points as a polyline: good enough to see the curve's shape, the
    // exact NURBS evaluation is not worth it for a viewer
    for(const auto &point : data->controllist)
        points << QPointF(point->x, point->y);
    if(points.size() < 2)
        return;
    prim.points = points;
    accumulateBounds(prim.transform.map(points));
    addPrim(std::move(prim));
}

void CadDrawing::addInsert(const DRW_Insert &data) {
    const QString name = QString::fromStdString(data.name);
    if(!mBlocks.contains(name) || mDepth >= kMaxInsertDepth)
        return;
    const QTransform blockTransform = mCurrentTransform;
    // rows/columns arrays of a MINSERT
    const int columns = std::max(1, data.colcount);
    const int rows = std::max(1, data.rowcount);
    for(int row = 0; row < rows; row++) {
        for(int column = 0; column < columns; column++) {
            QTransform transform;
            transform.translate(data.basePoint.x + column * data.colspace,
                                data.basePoint.y + row * data.rowspace);
            transform.rotate(data.angle * 180.0 / kPi);
            transform.scale(data.xscale != 0.0 ? data.xscale : 1.0,
                            data.yscale != 0.0 ? data.yscale : 1.0);
            // block coordinates are relative to the block's base point
            const QPointF base = mBlockBasePoints.value(name);
            transform.translate(-base.x(), -base.y());
            mDepth++;
            const QTransform saved = mCurrentTransform;
            mCurrentTransform = blockTransform * transform;
            // expand into the scene: temporarily pop the block so addPrim writes
            // to the scene instead of the block table
            const QString savedBlock = mCurrentBlock;
            mCurrentBlock.clear();
            const std::vector<Prim> blockPrims = mBlocks.value(name);
            for(const Prim &prim : blockPrims) {
                Prim copy = prim;
                copy.transform = mCurrentTransform * prim.transform;
                if(copy.kind == Prim::Kind::Poly || copy.kind == Prim::Kind::Solid) {
                    accumulateBounds(copy.transform.map(copy.points));
                } else if(copy.kind == Prim::Kind::Curve) {
                    QPolygonF box;
                    box << QPointF(copy.center.x() - copy.radiusX, copy.center.y() - copy.radiusY)
                        << QPointF(copy.center.x() + copy.radiusX, copy.center.y() + copy.radiusY);
                    accumulateBounds(copy.transform.map(box));
                }
                mPrimitives.push_back(std::move(copy));
            }
            mCurrentBlock = savedBlock;
            mCurrentTransform = saved;
            mDepth--;
        }
    }
}

void CadDrawing::addTrace(const DRW_Trace &data) {
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.points << toPoint(data.basePoint) << toPoint(data.secPoint)
                << toPoint(data.thirdPoint) << toPoint(data.fourPoint);
    if(data.secPoint.x == data.thirdPoint.x && data.secPoint.y == data.thirdPoint.y)
        prim.points.remove(2);
    prim.points << prim.points.first();
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    accumulateBounds(prim.transform.map(prim.points));
    addPrim(std::move(prim));
}

void CadDrawing::addSolid(const DRW_Solid &data) {
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.points << toPoint(data.basePoint) << toPoint(data.secPoint)
                << toPoint(data.thirdPoint);
    if(!(data.thirdPoint.x == data.fourPoint.x && data.thirdPoint.y == data.fourPoint.y))
        prim.points << toPoint(data.fourPoint);
    prim.points << prim.points.first();
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    accumulateBounds(prim.transform.map(prim.points));
    addPrim(std::move(prim));
}

void CadDrawing::addText(const DRW_Text &data) {
    Prim prim;
    prim.kind = Prim::Kind::Text;
    prim.text = QString::fromStdString(data.text);
    prim.textHeight = data.height > 0 ? data.height : 2.5;
    prim.textRotation = data.angle;
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    // text is drawn from its insertion point, so it does not add to the bounds by
    // its glyph size; the insertion point plus a nominal advance is enough
    const QPolygonF box{QPointF(data.basePoint.x, data.basePoint.y),
                        QPointF(data.basePoint.x + prim.textHeight * prim.text.size() * 0.6,
                                data.basePoint.y + prim.textHeight)};
    accumulateBounds(prim.transform.map(box));
    addPrim(std::move(prim));
}

void CadDrawing::addMText(const DRW_MText &data) {
    // keep the plain text: formatting codes would need a whole MTEXT parser
    QString text = QString::fromStdString(data.text);
    text.remove(QRegularExpression(QStringLiteral("\\\\[A-Za-z][^;]*;")));
    text.replace(QStringLiteral("\\\\P"), QStringLiteral("\n"));
    Prim prim;
    prim.kind = Prim::Kind::Text;
    prim.text = text;
    prim.textHeight = data.height > 0 ? data.height : 2.5;
    prim.textRotation = data.angle;
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    const QPolygonF box{QPointF(data.basePoint.x, data.basePoint.y),
                        QPointF(data.basePoint.x + prim.textHeight * text.size() * 0.6,
                                data.basePoint.y + prim.textHeight)};
    accumulateBounds(prim.transform.map(box));
    addPrim(std::move(prim));
}

void CadDrawing::addHatch(const DRW_Hatch *data) {
    if(!data)
        return;
    // The fill pattern itself is out of scope for a viewer, but the boundary is
    // what tells a filled area apart, so those loops are drawn as outlines.
    const QColor color = resolveColor(*data, Qt::black);
    for(const auto &loop : data->looplist) {
        if(!loop)
            continue;
        for(const auto &entity : loop->objlist) {
            if(!entity)
                continue;
            Prim prim;
            prim.kind = Prim::Kind::Poly;
            prim.color = color;
            prim.transform = mCurrentTransform;
            if(auto *line = dynamic_cast<DRW_Line *>(entity.get())) {
                prim.points << toPoint(line->basePoint) << toPoint(line->secPoint);
            } else if(auto *lw = dynamic_cast<DRW_LWPolyline *>(entity.get())) {
                for(const auto &vertex : lw->vertlist) {
                    if(vertex)
                        prim.points << QPointF(vertex->x, vertex->y);
                }
            } else if(auto *arc = dynamic_cast<DRW_Arc *>(entity.get())) {
                const double sweep = arc->endAngle() - arc->startAngle();
                const int segments = std::max(8, int(std::abs(sweep) / (kPi / 24.0)) + 1);
                for(int i = 0; i <= segments; i++) {
                    const double angle = arc->startAngle() + sweep * i / segments;
                    prim.points << toPoint(arc->basePoint)
                                       + QPointF(std::cos(angle), std::sin(angle)) * arc->radious;
                }
            }
            if(prim.points.size() < 2)
                continue;
            accumulateBounds(prim.transform.map(prim.points));
            addPrim(std::move(prim));
        }
    }
}

void CadDrawing::tessellateCurve(const Prim &prim, const QTransform &worldToImage, double pixelScale,
                                 QPolygonF &out) const {
    double sweep = prim.endAngle - prim.startAngle;
    if(std::abs(sweep) < 1e-9)
        return;
    const double worldRadius = prim.radiusX * prim.transform.m11();  // approximate scale
    const double deviceRadius = std::abs(worldRadius * pixelScale);
    int segments = int(std::abs(sweep) / (2.0 * kPi) * kMaxCircleSegments);
    segments = std::max(segments, int(std::abs(deviceRadius)));
    segments = std::clamp(segments, 8, kMaxCircleSegments);
    for(int i = 0; i <= segments; i++) {
        const double angle = prim.startAngle + sweep * i / segments;
        const QPointF local = prim.center + QPointF(std::cos(angle) * prim.radiusX,
                                                    std::sin(angle) * prim.radiusY);
        out << prim.transform.map(local);
    }
}

QImage CadDrawing::render(int width, int height, const QColor &background) const {
    QImage image(std::max(1, width), std::max(1, height), QImage::Format_ARGB32_Premultiplied);
    image.fill(background);
    if(mPrimitives.empty() || mBounds.isEmpty())
        return image;

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // fit the drawing into the image with a small margin, flipping Y (DXF is
    // y-up, images are y-down)
    const double margin = 0.02;
    const double availableWidth = image.width() * (1.0 - 2 * margin);
    const double availableHeight = image.height() * (1.0 - 2 * margin);
    const double scale = std::min(availableWidth / mBounds.width(), availableHeight / mBounds.height());
    const QPointF center = mBounds.center();

    QTransform worldToImage;
    worldToImage.translate(image.width() / 2.0, image.height() / 2.0);
    worldToImage.scale(scale, -scale);
    worldToImage.translate(-center.x(), -center.y());

    painter.setPen(QPen(Qt::black, 1.0));
    for(const Prim &prim : mPrimitives) {
        painter.setPen(QPen(prim.color, 1.0));
        if(prim.kind == Prim::Kind::Poly || prim.kind == Prim::Kind::Solid) {
            QPolygonF device = worldToImage.map(prim.transform.map(prim.points));
            painter.drawPolyline(device);
        } else if(prim.kind == Prim::Kind::Curve) {
            QPolygonF world;
            tessellateCurve(prim, worldToImage, scale, world);
            painter.drawPolyline(worldToImage.map(world));
        } else if(prim.kind == Prim::Kind::Text) {
            if(prim.text.isEmpty())
                continue;
            const QPointF position = worldToImage.map(prim.transform.map(QPointF()));
            const double pixelHeight = prim.textHeight * scale;
            if(pixelHeight < 2.0)
                continue;
            QFont font(QStringLiteral("DejaVu Sans"));
            font.setPixelSize(std::max(2, int(pixelHeight)));
            painter.setFont(font);
            painter.save();
            painter.translate(position);
            // text rotation follows the entity, mirrored because of the Y flip
            const double rotation = -prim.textRotation * 180.0 / kPi;
            painter.rotate(rotation);
            painter.drawText(QPointF(0, 0), prim.text);
            painter.restore();
        }
    }
    painter.end();
    return image;
}

void CadDrawing::add3dFace(const DRW_3Dface &data) {
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.points << toPoint(data.basePoint) << toPoint(data.secPoint)
                << toPoint(data.thirdPoint) << toPoint(data.fourPoint);
    prim.points << prim.points.first();
    prim.color = resolveColor(data, Qt::black);
    prim.transform = mCurrentTransform;
    accumulateBounds(prim.transform.map(prim.points));
    addPrim(std::move(prim));
}

void CadDrawing::addDimensionEntity(const DRW_Dimension *data) {
    if(!data)
        return;
    // A dimension's visible geometry is the block its CAD program generated; here
    // only the two definition points and the text position are known, so draw the
    // measurement line between them. It keeps dimensions locatable without
    // pretending to be a dimension renderer.
    Prim prim;
    prim.kind = Prim::Kind::Poly;
    prim.points << toPoint(data->getDefPoint()) << toPoint(data->getTextPoint());
    prim.color = resolveColor(*data, QColor(96, 96, 96));
    prim.transform = mCurrentTransform;
    accumulateBounds(prim.transform.map(prim.points));
    addPrim(std::move(prim));
}
