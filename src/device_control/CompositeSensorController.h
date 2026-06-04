#pragma once

#include "device_control/CompositeSensorCommandBuilder.h"
#include "device_control/CompositeSensorCommandParser.h"
#include "device_control/CompositeSensorState.h"
#include "foundation/config/ConfigTypes.h"
#include "foundation/error/Result.h"
#include "foundation/event/EventBus.h"
#include "hardware/serial/SerialPort.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tri::device_control {

class CompositeSensorController final {
public:
    CompositeSensorController() = default;
    ~CompositeSensorController();

    CompositeSensorController(const CompositeSensorController&) = delete;
    CompositeSensorController& operator=(const CompositeSensorController&) = delete;

    foundation::Result<void> init(const foundation::SerialConfig& config,
                                  foundation::EventBus* eventBus = nullptr);
    void shutdown();

    foundation::Result<void> setOutputMode(CompositeSensorOutputMode mode);
    foundation::Result<CompositeSensorOutputMode> queryOutputMode();
    foundation::Result<void> waitStable(int timeoutMs);

    foundation::Result<std::uint16_t> readRegister(std::uint16_t address);
    foundation::Result<void> writeRegister(std::uint16_t address, std::uint16_t value);
    foundation::Result<std::uint16_t> readRegister(CompositeSensorRegister reg);
    foundation::Result<void> writeRegister(CompositeSensorRegister reg, std::uint16_t value);

    foundation::Result<void> setFusionMode(CompositeSensorOutputMode mode);
    foundation::Result<CompositeSensorOutputMode> getFusionMode();
    foundation::Result<void> setFusionColor(FusionColor color);
    foundation::Result<void> setContourMode(ContourMode mode);
    foundation::Result<void> setInfraredPolarity(InfraredPolarity polarity);
    foundation::Result<void> triggerInfraredCorrection();
    foundation::Result<void> setInfraredBrightness(std::uint16_t value);
    foundation::Result<void> setInfraredContrast(std::uint16_t value);
    foundation::Result<void> setLowlightBrightness(std::uint16_t value);
    foundation::Result<void> setLowlightContrast(std::uint16_t value);
    foundation::Result<void> setInfraredRegistrationZoom(std::uint16_t value);
    foundation::Result<void> setInfraredRegistrationOffsetX(std::int16_t value);
    foundation::Result<void> setInfraredRegistrationOffsetY(std::int16_t value);
    foundation::Result<void> setLowlightRegistrationZoom(std::uint16_t value);
    foundation::Result<void> setLowlightRegistrationOffsetX(std::int16_t value);
    foundation::Result<void> setLowlightRegistrationOffsetY(std::int16_t value);
    foundation::Result<void> saveCurrentConfig();
    foundation::Result<void> restoreDefaultConfig();

    const CompositeSensorState& getState() const noexcept { return state_; }
    bool isReady() const noexcept { return state_.initialized && state_.serialOnline && serial_.isOpen(); }

    void setAckRequired(bool required) noexcept { ackRequired_ = required; }
    bool ackRequired() const noexcept { return ackRequired_; }

private:
    foundation::Result<void> ensureReady() const;
    foundation::Result<void> sendWriteCommandWithRetry(const std::vector<std::uint8_t>& command);
    foundation::Result<CompositeSensorAck> sendReadCommandWithRetry(const std::vector<std::uint8_t>& command);
    foundation::Result<void> sendWriteOnce(const std::vector<std::uint8_t>& command);
    foundation::Result<CompositeSensorAck> sendReadOnce(const std::vector<std::uint8_t>& command);
    foundation::Result<std::vector<std::uint8_t>> readExact(std::size_t expectedBytes, int timeoutMs);
    foundation::Result<void> validatePercent(std::uint16_t value, const char* name) const;
    void recordError(const foundation::Status& status);
    void publishModeChanged(CompositeSensorOutputMode mode);

    foundation::SerialConfig config_{};
    foundation::EventBus* eventBus_{nullptr};
    hardware::serial::SerialPort serial_;
    CompositeSensorCommandBuilder builder_;
    CompositeSensorCommandParser parser_;
    CompositeSensorState state_{};
    bool ackRequired_{true};
};

} // namespace tri::device_control
