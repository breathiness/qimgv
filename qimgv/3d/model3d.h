#pragma once

#include <QColor>
#include <QImage>
#include <QString>
#include <QVector3D>

#include <vector>

// A 3D model rendered into a QImage: assimp parses the file into triangles and a
// small software rasterizer paints them with a z-buffer and two directional
// lights. There is no interaction in here - the caller passes the orientation,
// so rotating is just another render call.
class Model3D {
public:
    struct View {
        // radians: yaw around the vertical axis, pitch around the horizontal one
        float yaw = 0.65f;
        float pitch = 0.45f;
        // 1.0 fits the model into the image, larger values zoom in
        float zoom = 1.0f;
    };

    // true when the build has assimp
    static bool isAvailable();

    bool load(const QString &filePath);
    bool isLoaded() const { return mLoaded; }
    QString errorString() const { return mError; }

    // bounding box size in model units; zero when nothing was loaded
    QVector3D size() const { return mSize; }
    int triangleCount() const { return int(mIndices.size() / 3); }

    // Paints the model into a width x height image with a transparent
    // background, antialiased by supersampling.
    QImage render(int width, int height, const View &view) const;

private:
    struct SubMesh {
        QColor color = QColor(180, 180, 185);
        int firstIndex = 0;
        int indexCount = 0;
    };

    bool mLoaded = false;
    QString mError;
    QVector3D mSize;
    QVector3D mCenter;
    float mRadius = 1.0f;
    std::vector<float> mPositions;   // x, y, z per vertex
    std::vector<float> mNormals;     // nx, ny, nz per vertex
    std::vector<unsigned int> mIndices;
    std::vector<SubMesh> mSubMeshes;
};
