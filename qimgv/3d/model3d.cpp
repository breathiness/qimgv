#include "model3d.h"

#include <QFile>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

#ifdef USE_ASSIMP
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#ifdef _WIN32
#include <windows.h>
#endif
#endif

#ifdef USE_ASSIMP
namespace {

// assimp is loaded at runtime, not linked: the library ships with the optional
// formats pack, and an installation without it must still start. The three C
// entry points are all the viewer needs.
struct AssimpApi {
    const aiScene *(*importFile)(const char *, unsigned int) = nullptr;
    void (*releaseImport)(const aiScene *) = nullptr;
    const char *(*getErrorString)() = nullptr;
    aiReturn (*getMaterialColor)(const aiMaterial *, const char *, unsigned int,
                                 unsigned int, aiColor4D *) = nullptr;
    bool available = false;

    AssimpApi()
    {
#ifdef _WIN32
        HMODULE module = LoadLibraryW(L"libassimp-6.dll");
        if(!module)
            module = LoadLibraryW(L"assimp6.dll");
        if(!module)
            module = LoadLibraryW(L"assimp.dll");
        if(!module)
            return;
        importFile = reinterpret_cast<decltype(importFile)>(GetProcAddress(module, "aiImportFile"));
        releaseImport = reinterpret_cast<decltype(releaseImport)>(GetProcAddress(module, "aiReleaseImport"));
        getErrorString = reinterpret_cast<decltype(getErrorString)>(GetProcAddress(module, "aiGetErrorString"));
        getMaterialColor = reinterpret_cast<decltype(getMaterialColor)>(
            GetProcAddress(module, "aiGetMaterialColor"));
        available = importFile && releaseImport && getErrorString && getMaterialColor;
#endif
    }
};

const AssimpApi &api()
{
    static const AssimpApi instance;
    return instance;
}

// A flat colour per submesh is all the rasterizer needs from a material.
QColor materialColor(const aiMaterial *material)
{
    if(material) {
        aiColor4D diffuse;
        if(api().getMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS) {
            const QColor color(int(qBound(0.0f, diffuse.r, 1.0f) * 255.0f),
                               int(qBound(0.0f, diffuse.g, 1.0f) * 255.0f),
                               int(qBound(0.0f, diffuse.b, 1.0f) * 255.0f));
            if(color.isValid())
                return color;
        }
    }
    return QColor(178, 180, 186);
}

} // namespace
#endif

bool Model3D::isAvailable()
{
#ifdef USE_ASSIMP
    return api().available;
#else
    return false;
#endif
}

bool Model3D::load(const QString &filePath)
{
    mLoaded = false;
    mError.clear();
    mPositions.clear();
    mNormals.clear();
    mIndices.clear();
    mSubMeshes.clear();

#ifdef USE_ASSIMP
    if(!api().available) {
        mError = QStringLiteral("assimp library not found");
        return false;
    }
    // PreTransformVertices flattens the node hierarchy (the viewer has no
    // animation or instancing), Triangulate makes the rasterizer's life easy and
    // GenSmoothNormals covers STL files that carry no normals at all.
    const aiScene *scene = api().importFile(
        QFile::encodeName(filePath).constData(),
        aiProcess_Triangulate | aiProcess_PreTransformVertices
            | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals
            | aiProcess_SortByPType);
    if(!scene || !scene->mRootNode) {
        mError = QString::fromUtf8(api().getErrorString());
        return false;
    }

    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
    QVector3D maximum(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                      -std::numeric_limits<float>::max());

    for(unsigned int m = 0; m < scene->mNumMeshes; m++) {
        const aiMesh *mesh = scene->mMeshes[m];
        if(!mesh || mesh->mNumVertices == 0)
            continue;
        SubMesh subMesh;
        subMesh.color = materialColor(scene->mMaterials ? scene->mMaterials[mesh->mMaterialIndex] : nullptr);
        subMesh.firstIndex = int(mIndices.size());

        const unsigned int base = unsigned(mPositions.size() / 3);
        for(unsigned int v = 0; v < mesh->mNumVertices; v++) {
            const aiVector3D &p = mesh->mVertices[v];
            mPositions.push_back(p.x);
            mPositions.push_back(p.y);
            mPositions.push_back(p.z);

            aiVector3D n = mesh->mNormals ? mesh->mNormals[v] : aiVector3D(0.0f, 0.0f, 1.0f);
            const ai_real length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if(length > 1e-6f) {
                n.x /= length;
                n.y /= length;
                n.z /= length;
            } else {
                n = aiVector3D(0.0f, 0.0f, 1.0f);
            }
            mNormals.push_back(n.x);
            mNormals.push_back(n.y);
            mNormals.push_back(n.z);

            minimum.setX(qMin(minimum.x(), p.x));
            minimum.setY(qMin(minimum.y(), p.y));
            minimum.setZ(qMin(minimum.z(), p.z));
            maximum.setX(qMax(maximum.x(), p.x));
            maximum.setY(qMax(maximum.y(), p.y));
            maximum.setZ(qMax(maximum.z(), p.z));
        }

        // triangles only: SortByPType + Triangulate leave points and lines out of
        // the face list, and the face size is always 3 here
        for(unsigned int f = 0; f < mesh->mNumFaces; f++) {
            const aiFace &face = mesh->mFaces[f];
            if(face.mNumIndices != 3)
                continue;
            mIndices.push_back(base + face.mIndices[0]);
            mIndices.push_back(base + face.mIndices[1]);
            mIndices.push_back(base + face.mIndices[2]);
        }
        subMesh.indexCount = int(mIndices.size()) - subMesh.firstIndex;
        if(subMesh.indexCount > 0)
            mSubMeshes.push_back(subMesh);
    }

    if(mIndices.empty() || mPositions.empty()) {
        api().releaseImport(scene);
        mError = QStringLiteral("no triangles");
        return false;
    }

    mSize = maximum - minimum;
    mCenter = (minimum + maximum) * 0.5f;
    float radiusSquared = 0.0f;
    for(size_t i = 0; i < mPositions.size(); i += 3) {
        const QVector3D p(mPositions[i], mPositions[i + 1], mPositions[i + 2]);
        radiusSquared = qMax(radiusSquared, (p - mCenter).lengthSquared());
    }
    mRadius = std::sqrt(radiusSquared);
    if(mRadius <= 1e-6f)
        mRadius = 1.0f;

    api().releaseImport(scene);
    mLoaded = true;
    return true;
#else
    mError = QStringLiteral("this build has no assimp");
    return false;
#endif
}

QImage Model3D::render(int width, int height, const View &view) const
{
    if(!mLoaded || width <= 0 || height <= 0 || mIndices.empty())
        return QImage();

    // Supersampling smooths the triangle edges; big images and huge models pay
    // for it with memory, so they are rendered at native resolution.
    const qint64 pixels = qint64(width) * height;
    const int superSample = (pixels <= 2'000'000 && triangleCount() <= 1'500'000) ? 2 : 1;
    const int w = width * superSample;
    const int h = height * superSample;
    if(qint64(w) * h > 36'000'000)
        return QImage();

    std::vector<float> depthBuffer(size_t(w) * size_t(h), std::numeric_limits<float>::max());
    QImage canvas(w, h, QImage::Format_ARGB32_Premultiplied);
    if(canvas.isNull())
        return QImage();
    canvas.fill(Qt::transparent);
    quint32 *pixelsOut = reinterpret_cast<quint32 *>(canvas.bits());
    const int stridePixels = canvas.bytesPerLine() / 4;

    const float cosYaw = std::cos(view.yaw);
    const float sinYaw = std::sin(view.yaw);
    const float cosPitch = std::cos(view.pitch);
    const float sinPitch = std::sin(view.pitch);

    auto rotate = [&](float x, float y, float z, float &rx, float &ry, float &rz) {
        const float x1 = cosYaw * x + sinYaw * z;
        const float z1 = -sinYaw * x + cosYaw * z;
        const float y1 = cosPitch * y - sinPitch * z1;
        const float z2 = sinPitch * y + cosPitch * z1;
        rx = x1;
        ry = y1;
        rz = z2;
    };

    const float scale = std::min(w, h) * 0.45f / mRadius * qMax(0.05f, view.zoom);
    const float centerX = w * 0.5f;
    const float centerY = h * 0.5f;

    const size_t vertexCount = mPositions.size() / 3;
    std::vector<float> screenX(vertexCount), screenY(vertexCount), screenZ(vertexCount);
    std::vector<float> lightA(vertexCount), lightB(vertexCount);
    // two directional lights, fixed in camera space
    const QVector3D light1 = QVector3D(-0.35f, 0.55f, 0.76f).normalized();
    const QVector3D light2 = QVector3D(0.62f, -0.42f, 0.66f).normalized();
    auto shadeOf = [](const QVector3D &normal, const QVector3D &l1, const QVector3D &l2) {
        const float a = qMax(0.0f, QVector3D::dotProduct(normal, l1));
        const float b = qMax(0.0f, QVector3D::dotProduct(normal, l2));
        return qBound(0.12f, 0.26f + 0.62f * a + 0.34f * b, 1.0f);
    };

    for(size_t i = 0; i < vertexCount; i++) {
        float px, py, pz;
        rotate(mPositions[i * 3] - mCenter.x(), mPositions[i * 3 + 1] - mCenter.y(),
               mPositions[i * 3 + 2] - mCenter.z(), px, py, pz);
        screenX[i] = centerX + px * scale;
        screenY[i] = centerY - py * scale;
        screenZ[i] = pz;

        float nx, ny, nz;
        rotate(mNormals[i * 3], mNormals[i * 3 + 1], mNormals[i * 3 + 2], nx, ny, nz);
        const QVector3D normal(nx, ny, nz);
        lightA[i] = shadeOf(normal, light1, light2);
        lightB[i] = shadeOf(-normal, light1, light2);
    }

    for(const SubMesh &subMesh : mSubMeshes) {
        const int r = subMesh.color.red();
        const int g = subMesh.color.green();
        const int b = subMesh.color.blue();
        for(int i = subMesh.firstIndex; i + 2 < subMesh.firstIndex + subMesh.indexCount; i += 3) {
            const unsigned int i0 = mIndices[i];
            const unsigned int i1 = mIndices[i + 1];
            const unsigned int i2 = mIndices[i + 2];
            if(i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
                continue;

            const float x0 = screenX[i0], y0 = screenY[i0], z0 = screenZ[i0];
            const float x1 = screenX[i1], y1 = screenY[i1], z1 = screenZ[i1];
            const float x2 = screenX[i2], y2 = screenY[i2], z2 = screenZ[i2];

            const float denominator = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
            if(std::fabs(denominator) < 1e-9f)
                continue;

            int minX = int(std::floor(qMin(x0, qMin(x1, x2))));
            int maxX = int(std::ceil(qMax(x0, qMax(x1, x2))));
            int minY = int(std::floor(qMin(y0, qMin(y1, y2))));
            int maxY = int(std::ceil(qMax(y0, qMax(y1, y2))));
            minX = qBound(0, minX, w - 1);
            maxX = qBound(0, maxX, w - 1);
            minY = qBound(0, minY, h - 1);
            maxY = qBound(0, maxY, h - 1);
            if(minX > maxX || minY > maxY)
                continue;

            // back faces get the flipped normals, so an inverted or open mesh is
            // not simply black
            const bool flip = denominator > 0.0f;
            const float sh0 = flip ? lightB[i0] : lightA[i0];
            const float sh1 = flip ? lightB[i1] : lightA[i1];
            const float sh2 = flip ? lightB[i2] : lightA[i2];
            const int cr0 = int(r * sh0), cg0 = int(g * sh0), cb0 = int(b * sh0);
            const int cr1 = int(r * sh1), cg1 = int(g * sh1), cb1 = int(b * sh1);
            const int cr2 = int(r * sh2), cg2 = int(g * sh2), cb2 = int(b * sh2);

            const float invDenominator = 1.0f / denominator;
            for(int py = minY; py <= maxY; py++) {
                const float fy = float(py);
                quint32 *row = pixelsOut + size_t(py) * stridePixels;
                float *depthRow = depthBuffer.data() + size_t(py) * w;
                for(int px = minX; px <= maxX; px++) {
                    const float fx = float(px);
                    const float w0 = ((y1 - y2) * (fx - x2) + (x2 - x1) * (fy - y2)) * invDenominator;
                    const float w1 = ((y2 - y0) * (fx - x2) + (x0 - x2) * (fy - y2)) * invDenominator;
                    const float w2 = 1.0f - w0 - w1;
                    if(w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                        continue;
                    const float z = w0 * z0 + w1 * z1 + w2 * z2;
                    if(z >= depthRow[px])
                        continue;
                    depthRow[px] = z;
                    const int cr = int(w0 * cr0 + w1 * cr1 + w2 * cr2);
                    const int cg = int(w0 * cg0 + w1 * cg1 + w2 * cg2);
                    const int cb = int(w0 * cb0 + w1 * cb1 + w2 * cb2);
                    row[px] = qRgba(qBound(0, cr, 255), qBound(0, cg, 255), qBound(0, cb, 255), 255);
                }
            }
        }
    }

    if(superSample == 1)
        return canvas;
    return canvas.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
