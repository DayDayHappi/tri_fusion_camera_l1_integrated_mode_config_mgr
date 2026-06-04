#pragma once

#include "algorithm/registration/RegistrationTypes.h"
#include "foundation/error/Result.h"
#include "media/frame/VideoFrame.h"

namespace tri::algorithm::registration {

class HomographyWarper final {
public:
    foundation::Result<media::VideoFrame> warp(const media::VideoFrame& frame,
                                               const HomographyMatrix& matrix,
                                               ImageSize outputSize) const {
        (void)matrix;
        (void)outputSize;
        // 空实现：当前只保留接口，后续可接 RGA/OpenCV/RKNN 前处理。
        return foundation::Result<media::VideoFrame>::ok(frame);
    }
};

} // namespace tri::algorithm::registration
