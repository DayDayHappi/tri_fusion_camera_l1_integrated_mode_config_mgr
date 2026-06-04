#pragma once

#include "algorithm/registration/CalibrationManager.h"
#include "algorithm/registration/HomographyWarper.h"
#include "foundation/error/Result.h"
#include "media/frame/SyncedFrameGroup.h"

namespace tri::algorithm::registration {

class ImageAligner final {
public:
    foundation::Result<void> init(const CalibrationProfile& profile);
    foundation::Result<media::SyncedFrameGroup> align(const media::SyncedFrameGroup& group) const;
    bool initialized() const noexcept { return initialized_; }

private:
    CalibrationProfile profile_{};
    HomographyWarper warper_{};
    bool initialized_{false};
};

} // namespace tri::algorithm::registration
