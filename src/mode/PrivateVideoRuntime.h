#pragma once

#include "media/app_fusion/AppFusionPipeline.h"
#include "media/gstreamer/GStreamerPipeline.h"
#include "media/gstreamer/GStreamerPipelineConfigManager.h"
#include "protocol/private/PrivateControlServer.h"

#include <memory>
#include <string>

namespace tri::mode {

class PrivateVideoRuntime {
public:
    explicit PrivateVideoRuntime(tri::media::gstreamer::GStreamerPipelineConfigManager& configManager);
    ~PrivateVideoRuntime();

    PrivateVideoRuntime(const PrivateVideoRuntime&) = delete;
    PrivateVideoRuntime& operator=(const PrivateVideoRuntime&) = delete;

    bool start(tri::protocol::private_api::PrivateWorkMode mode);
    bool switchTo(tri::protocol::private_api::PrivateWorkMode mode);
    bool stop();
    bool isRunning() const;

    tri::protocol::private_api::PrivateWorkMode currentMode() const;
    std::string lastError() const;
    std::string activeDescription() const;

private:
    bool startGstLaunchMode(tri::protocol::private_api::PrivateWorkMode mode);
    bool startAppFusionMode(tri::protocol::private_api::PrivateWorkMode mode);
    tri::media::app_fusion::AppFusionOptions buildAppFusionOptions(
        tri::protocol::private_api::PrivateWorkMode mode) const;

private:
    tri::media::gstreamer::GStreamerPipelineConfigManager& configManager_;
    std::unique_ptr<tri::media::gstreamer::GStreamerPipeline> gstPipeline_;
    tri::media::app_fusion::AppFusionPipeline appFusionPipeline_;
    tri::protocol::private_api::PrivateWorkMode currentMode_;
    bool hasMode_{false};
    bool usingAppFusion_{false};
    std::string lastError_;
    std::string activeDescription_;
};

} // namespace tri::mode
