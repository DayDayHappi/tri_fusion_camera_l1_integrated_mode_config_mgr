#include "algorithm/registration/ImageAligner.h"
#include "foundation/error/ErrorCode.h"
#include <utility>

namespace tri::algorithm::registration {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> ImageAligner::init(const CalibrationProfile& profile) {
    if (!profile.valid) {
        initialized_ = false;
        return Result<void>::success();
    }
    profile_ = profile;
    initialized_ = true;
    return Result<void>::success();
}

Result<media::SyncedFrameGroup> ImageAligner::align(const media::SyncedFrameGroup& group) const {
    if (!initialized_) {
        return Result<media::SyncedFrameGroup>::ok(group);
    }

    auto out = group;
    auto warped = warper_.warp(group.visible, profile_.visibleToComposite, profile_.compositeSize);
    if (!warped) {
        return Result<media::SyncedFrameGroup>::error(warped.status().code(), warped.status().describe());
    }
    out.visible = warped.value();
    return Result<media::SyncedFrameGroup>::ok(std::move(out));
}

} // namespace tri::algorithm::registration
