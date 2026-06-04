#include "algorithm/registration/CalibrationManager.h"
#include "foundation/error/ErrorCode.h"
#include <utility>

namespace tri::algorithm::registration {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> CalibrationManager::load(const std::string& path) {
    if (path.empty()) {
        profile_ = CalibrationProfile{};
        return Result<void>::success();
    }

    // L5 第一版只定义标定接入点，不解析具体标定文件格式。
    // 后续可在这里接入 YAML/JSON 标定矩阵读取。
    profile_ = CalibrationProfile{};
    profile_.name = path;
    profile_.valid = false;
    return Result<void>::error(ErrorCode::Unsupported,
                               "calibration file parsing is not implemented yet: " + path);
}

Result<void> CalibrationManager::setProfile(CalibrationProfile profile) {
    if (!profile.valid) {
        return Result<void>::error(ErrorCode::InvalidArgument, "calibration profile is invalid");
    }
    profile_ = std::move(profile);
    return Result<void>::success();
}

} // namespace tri::algorithm::registration
