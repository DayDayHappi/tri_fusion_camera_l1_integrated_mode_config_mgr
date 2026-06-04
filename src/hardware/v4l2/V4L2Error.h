#pragma once
#include "foundation/error/ErrorCode.h"

namespace tri::hardware::v4l2 {

inline tri::foundation::ErrorCode openFailedCodeForPath(const std::string& path) {
    return path.find("video0") != std::string::npos ? tri::foundation::ErrorCode::CompositeCameraOpenFailed
                                                     : tri::foundation::ErrorCode::IoError;
}

} // namespace tri::hardware::v4l2
