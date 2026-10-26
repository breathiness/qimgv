#pragma once

#include "utils/imagelib.h"
#include "sourcecontainers/documentinfo.h"
#include "sourcecontainers/image.h"
#include "sourcecontainers/imageanimated.h"
#include "sourcecontainers/imagestatic.h"
#include "sourcecontainers/video.h"

class ImageFactory {
public:
    // previewTarget: decode only a downscaled copy first (see ImageStatic);
    // empty means load everything in one go
    static std::shared_ptr<Image> createImage(QString path, QSize previewTarget = QSize());
};
