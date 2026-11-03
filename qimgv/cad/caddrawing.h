#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <vector>

#include "drw_interface.h"

// Reads a DXF (or DWG) drawing through libdxfrw, keeps a small scene of
// primitives and paints it into a QImage. Only the 2D geometry an image viewer
// needs is kept: no editing, no document structure beyond blocks.
//
// The scene is kept in drawing units; every primitive carries the transform that
// BLOCK/INSERT nesting applied to it, so inserting a block does not flatten it
// until paint time.
class CadDrawing : public DRW_Interface {
public:
    struct Prim {
        enum class Kind { Poly, Curve, Text, Solid };
        Kind kind = Kind::Poly;
        // Poly: the points; Solid: three or four corners
        QPolygonF points;
        // Curve: an arc of an ellipse in local units
        QPointF center;
        double radiusX = 0.0;
        double radiusY = 0.0;
        double startAngle = 0.0;
        double endAngle = 0.0;
        // Text
        QString text;
        double textHeight = 1.0;
        double textRotation = 0.0;
        // local -> drawing units (block insert scale/rotation)
        QTransform transform;
        QColor color = Qt::black;
    };

    bool load(const QString &filePath);
    bool isLoaded() const { return mLoaded; }
    QString errorString() const { return mError; }

    // bounding box of everything in drawing units; empty when nothing was read
    QRectF bounds() const { return mBounds; }
    int primitiveCount() const { return int(mPrimitives.size()); }

    // fits the drawing into width x height, keeping the aspect ratio
    QImage render(int width, int height, const QColor &background = Qt::white) const;

    // DRW_Interface: tables
    void addHeader(const DRW_Header *data) override { Q_UNUSED(data) }
    void addLType(const DRW_LType &data) override { Q_UNUSED(data) }
    void addLayer(const DRW_Layer &data) override;
    void addDimStyle(const DRW_Dimstyle &data) override { Q_UNUSED(data) }
    void addVport(const DRW_Vport &data) override { Q_UNUSED(data) }
    void addTextStyle(const DRW_Textstyle &data) override { Q_UNUSED(data) }
    void addAppId(const DRW_AppId &data) override { Q_UNUSED(data) }
    void addBlockRecord(const DRW_Block_Record &data) override { Q_UNUSED(data) }

    // DRW_Interface: blocks and entities
    void addBlock(const DRW_Block &data) override;
    void endBlock() override;
    void addPoint(const DRW_Point &data) override;
    void addLine(const DRW_Line &data) override;
    void addRay(const DRW_Ray &data) override;
    void addXline(const DRW_Xline &data) override;
    void addArc(const DRW_Arc &data) override;
    void addCircle(const DRW_Circle &data) override;
    void addEllipse(const DRW_Ellipse &data) override;
    void addLWPolyline(const DRW_LWPolyline &data) override;
    void addPolyline(const DRW_Polyline &data) override;
    void addSpline(const DRW_Spline *data) override;
    void addKnot(const DRW_Entity &data) override { Q_UNUSED(data) }
    void setBlock(int handle) override { Q_UNUSED(handle) }
    void add3dFace(const DRW_3Dface &data) override;
    // dimensions carry their geometry as an anonymous block; libdxfrw hands them
    // over as their own entity types
    void addDimAlign(const DRW_DimAligned *data) override { addDimensionEntity(data); }
    void addDimLinear(const DRW_DimLinear *data) override { addDimensionEntity(data); }
    void addDimRadial(const DRW_DimRadial *data) override { addDimensionEntity(data); }
    void addDimDiametric(const DRW_DimDiametric *data) override { addDimensionEntity(data); }
    void addDimAngular(const DRW_DimAngular *data) override { addDimensionEntity(data); }
    void addDimAngular3P(const DRW_DimAngular3p *data) override { addDimensionEntity(data); }
    void addDimOrdinate(const DRW_DimOrdinate *data) override { addDimensionEntity(data); }
    void addInsert(const DRW_Insert &data) override;
    void addTrace(const DRW_Trace &data) override;
    void addSolid(const DRW_Solid &data) override;
    void addMText(const DRW_MText &data) override;
    void addText(const DRW_Text &data) override;
    void addHatch(const DRW_Hatch *data) override;
    void addLeader(const DRW_Leader *data) override { Q_UNUSED(data) }
    // dimensions: draw the definition points as a thin line so they are visible at
    // all; the real geometry lives in the block a CAD program builds for them
    void addDimensionEntity(const DRW_Dimension *data);
    void addViewport(const DRW_Viewport &data) override { Q_UNUSED(data) }
    void addImage(const DRW_Image *data) override { Q_UNUSED(data) }
    void linkImage(const DRW_ImageDef *data) override { Q_UNUSED(data) }
    void addComment(const char *comment) override { Q_UNUSED(comment) }
    void addPlotSettings(const DRW_PlotSettings *data) override { Q_UNUSED(data) }

    // DRW_Interface: writing, unused
    void writeHeader(DRW_Header &data) override { Q_UNUSED(data) }
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}

private:
    void addPrim(Prim prim);
    void computeBounds();
    void accumulateBounds(const QPolygonF &worldPoints);
    // expands an INSERT into the scene by replaying the stored block
    void instantiateBlock(const QString &name, const QTransform &transform, int depth);
    QColor resolveColor(const DRW_Entity &entity, QColor fallback) const;
    void tessellateCurve(const Prim &prim, const QTransform &worldToImage, double pixelScale,
                         QPolygonF &out) const;

    bool mLoaded = false;
    bool mHasBounds = false;
    QString mError;
    QRectF mBounds;
    std::vector<Prim> mPrimitives;
    // block name -> primitives in block coordinates
    QHash<QString, std::vector<Prim>> mBlocks;
    // insert coordinates are relative to the block's own base point
    QHash<QString, QPointF> mBlockBasePoints;
    QHash<QString, QColor> mLayerColors;
    // the block currently being read, and the transform for its contents
    QString mCurrentBlock;
    QTransform mCurrentTransform;
    int mDepth = 0;
};
