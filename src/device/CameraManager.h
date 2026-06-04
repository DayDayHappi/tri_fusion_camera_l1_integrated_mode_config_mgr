#pragma once

#include "device/CompositeLowThermalCameraDevice.h"
#include "device/VisibleCameraDevice.h"
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"

#include <memory>
#include <vector>

namespace tri::device {

class CameraManager final {
public:
    CameraManager();
    ~CameraManager();

    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    foundation::Result<void> init(const foundation::CameraConfig& config,
                                  foundation::EventBus* eventBus = nullptr);
    foundation::Result<void> openAllEnabled();
    foundation::Result<void> startCamera(CameraId id);
    foundation::Result<void> stopCamera(CameraId id);
    void stopAll();
    void closeAll();

    CameraDevice* camera(CameraId id) noexcept;
    const CameraDevice* camera(CameraId id) const noexcept;
    VisibleCameraDevice* visibleCamera() noexcept { return visible_.get(); }
    CompositeLowThermalCameraDevice* compositeCamera() noexcept { return composite_.get(); }

    std::vector<CameraStatus> queryStatus() const;

private:
    CameraDevice* select(CameraId id) noexcept;
    const CameraDevice* select(CameraId id) const noexcept;
    void publishCameraEvent(const CameraStatus& status, bool online);

    foundation::CameraConfig config_{};
    foundation::EventBus* eventBus_{nullptr};
    std::unique_ptr<VisibleCameraDevice> visible_;
    std::unique_ptr<CompositeLowThermalCameraDevice> composite_;
    bool initialized_{false};
};

} // namespace tri::device
