#pragma once

#include "algorithm/FusionTypes.h"
#include "foundation/error/Result.h"
#include "media/frame/SyncedFrameGroup.h"
#include "media/frame/VideoFrame.h"

#include <string>

namespace tri::algorithm {

class FusionAlgorithm {
public:
    virtual ~FusionAlgorithm() = default;

    FusionAlgorithm(const FusionAlgorithm&) = delete;
    FusionAlgorithm& operator=(const FusionAlgorithm&) = delete;

    virtual FusionMode mode() const noexcept = 0;
    virtual const char* name() const noexcept = 0;

    virtual foundation::Result<void> init(const FusionConfig& config) = 0;
    virtual foundation::Result<media::VideoFrame> process(const media::SyncedFrameGroup& input) = 0;
    virtual foundation::Result<void> setParam(const std::string& key, const std::string& value) = 0;
    virtual foundation::Result<std::string> getParam(const std::string& key) const = 0;
    virtual void release() = 0;

protected:
    FusionAlgorithm() = default;
};

} // namespace tri::algorithm
