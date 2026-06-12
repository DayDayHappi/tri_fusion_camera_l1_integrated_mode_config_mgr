#include "device_control/CompositeSensorController.h"
#include "foundation/error/ErrorCode.h"
#include "foundation/event/Event.h"
#include "foundation/log/Logger.h"
#include "foundation/time/Clock.h"
#include "foundation/utils/ByteUtils.h"
#include "hardware/common/HardwareConfig.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace tri::device_control {
using tri::foundation::ErrorCode;
using tri::foundation::Event;
using tri::foundation::EventType;
using tri::foundation::LogCategory;
using tri::foundation::Result;
using tri::foundation::Status;

namespace {
constexpr std::size_t kWriteAckBytes = 10;
constexpr std::size_t kReadAckBytes = 12;
}

CompositeSensorController::~CompositeSensorController() {
    shutdown();
}

Result<void> CompositeSensorController::init(const foundation::SerialConfig& config,
                                             foundation::EventBus* eventBus) {
    if (state_.initialized) {
        return Result<void>::error(ErrorCode::AlreadyInitialized,
                                   "composite sensor controller already initialized");
    }

    config_ = config;
    eventBus_ = eventBus;

    auto builderRet = builder_.init(config_);
    if (!builderRet) {
        recordError(builderRet.status());
        return builderRet;
    }

    auto portCfg = tri::hardware::makeSerialPortConfig(config_);
    if (!portCfg) {
        recordError(portCfg.status());
        return Result<void>::error(portCfg.status().code(), portCfg.status().describe());
    }

    auto openRet = serial_.openPort(portCfg.value());
    if (!openRet) {
        recordError(openRet.status());
        return openRet;
    }

    state_.initialized = true;
    state_.serialOnline = true;
    state_.lastError.clear();

    TRI_LOG_INFO(LogCategory::Serial) << "composite sensor controller initialized on "
                                      << portCfg.value().device;
    return Result<void>::success();
}

void CompositeSensorController::shutdown() {
    serial_.closePort();
    state_.serialOnline = false;
    state_.initialized = false;
}

Result<void> CompositeSensorController::setOutputMode(CompositeSensorOutputMode mode) {
    return setFusionMode(mode);
}

Result<void> CompositeSensorController::setFusionMode(CompositeSensorOutputMode mode) {
    if (mode == CompositeSensorOutputMode::Unknown) {
        return Result<void>::error(ErrorCode::InvalidArgument, "invalid composite sensor output mode");
    }

    const auto value = outputModeRegisterValue(mode);
    auto ret = writeRegister(CompositeSensorRegister::FusionMode, value);
    if (!ret) return ret;

    state_.currentMode = mode;
    state_.lastSwitchTimeMs = tri::foundation::Clock::nowMs();
    ++state_.switchCount;
    state_.lastError.clear();

    publishModeChanged(mode);
    return Result<void>::success();
}

Result<CompositeSensorOutputMode> CompositeSensorController::queryOutputMode() {
    return getFusionMode();
}

Result<CompositeSensorOutputMode> CompositeSensorController::getFusionMode() {
    auto valueRet = readRegister(CompositeSensorRegister::FusionMode);
    if (!valueRet) {
        return Result<CompositeSensorOutputMode>::error(valueRet.status().code(), valueRet.status().describe());
    }

    auto mode = outputModeFromRegisterValue(valueRet.value());
    if (mode == CompositeSensorOutputMode::Unknown) {
        return Result<CompositeSensorOutputMode>::error(ErrorCode::SerialAckInvalid,
                                                        "unknown fusion mode register value: " +
                                                        std::to_string(valueRet.value()));
    }

    state_.currentMode = mode;
    return Result<CompositeSensorOutputMode>::ok(mode);
}

Result<void> CompositeSensorController::waitStable(int timeoutMs) {
    if (timeoutMs < 0) {
        return Result<void>::error(ErrorCode::InvalidArgument, "timeoutMs must be >= 0");
    }
    auto readyRet = ensureReady();
    if (!readyRet) return readyRet;

    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
    return Result<void>::success();
}

Result<std::uint16_t> CompositeSensorController::readRegister(CompositeSensorRegister reg) {
    return readRegister(static_cast<std::uint16_t>(reg));
}

Result<void> CompositeSensorController::writeRegister(CompositeSensorRegister reg, std::uint16_t value) {
    return writeRegister(static_cast<std::uint16_t>(reg), value);
}

Result<std::uint16_t> CompositeSensorController::readRegister(std::uint16_t address) {
    auto readyRet = ensureReady();
    if (!readyRet) {
        return Result<std::uint16_t>::error(readyRet.status().code(), readyRet.status().describe());
    }

    auto cmd = builder_.buildReadRegister(address);
    if (!cmd) {
        recordError(cmd.status());
        return Result<std::uint16_t>::error(cmd.status().code(), cmd.status().describe());
    }

    TRI_LOG_INFO(LogCategory::Serial) << "read composite sensor register addr=0x"
                                      << std::hex << address << std::dec
                                      << ", command=" << tri::foundation::bytes::toHex(cmd.value());

    auto ackRet = sendReadCommandWithRetry(cmd.value());
    if (!ackRet) {
        recordError(ackRet.status());
        return Result<std::uint16_t>::error(ackRet.status().code(), ackRet.status().describe());
    }

    state_.lastError.clear();
    return Result<std::uint16_t>::ok(ackRet.value().data);
}

Result<void> CompositeSensorController::writeRegister(std::uint16_t address, std::uint16_t value) {
    auto readyRet = ensureReady();
    if (!readyRet) return readyRet;

    auto cmd = builder_.buildWriteRegister(address, value);
    if (!cmd) {
        recordError(cmd.status());
        return Result<void>::error(cmd.status().code(), cmd.status().describe());
    }

    TRI_LOG_INFO(LogCategory::Serial) << "write composite sensor register addr=0x"
                                      << std::hex << address << ", value=0x" << value << std::dec
                                      << ", command=" << tri::foundation::bytes::toHex(cmd.value());

    auto ret = sendWriteCommandWithRetry(cmd.value());
    if (!ret) {
        recordError(ret.status());
        return ret;
    }

    state_.lastError.clear();
    return Result<void>::success();
}

Result<void> CompositeSensorController::setFusionColor(FusionColor color) {
    return writeRegister(CompositeSensorRegister::FusionColor, static_cast<std::uint16_t>(color));
}

Result<void> CompositeSensorController::setInfraredPolarity(InfraredPolarity polarity) {
    return writeRegister(CompositeSensorRegister::InfraredPolarity, static_cast<std::uint16_t>(polarity));
}

Result<void> CompositeSensorController::setContourMode(ContourMode mode) {
    return writeRegister(CompositeSensorRegister::ContourMode, static_cast<std::uint16_t>(mode));
}

Result<void> CompositeSensorController::triggerInfraredCorrection() {
    return writeRegister(CompositeSensorRegister::InfraredCorrection, 1);
}

Result<void> CompositeSensorController::setInfraredBrightness(std::uint16_t value) {
    auto ret = validatePercent(value, "infrared brightness");
    if (!ret) return ret;
    return writeRegister(CompositeSensorRegister::InfraredBrightness, value);
}

Result<void> CompositeSensorController::setInfraredContrast(std::uint16_t value) {
    auto ret = validatePercent(value, "infrared contrast");
    if (!ret) return ret;
    return writeRegister(CompositeSensorRegister::InfraredContrast, value);
}

Result<void> CompositeSensorController::setLowlightBrightness(std::uint16_t value) {
    auto ret = validatePercent(value, "lowlight brightness");
    if (!ret) return ret;
    return writeRegister(CompositeSensorRegister::LowlightBrightness, value);
}

Result<void> CompositeSensorController::setLowlightContrast(std::uint16_t value) {
    auto ret = validatePercent(value, "lowlight contrast");
    if (!ret) return ret;
    return writeRegister(CompositeSensorRegister::LowlightContrast, value);
}

Result<void> CompositeSensorController::setInfraredRegistrationZoom(std::uint16_t value) {
    if (value > 15) return Result<void>::error(ErrorCode::InvalidArgument, "infrared registration zoom must be 0..15");
    return writeRegister(CompositeSensorRegister::InfraredRegistrationZoom, value);
}

Result<void> CompositeSensorController::setInfraredRegistrationOffsetX(std::int16_t value) {
    return writeRegister(CompositeSensorRegister::InfraredRegistrationOffsetX,
                         static_cast<std::uint16_t>(value));
}

Result<void> CompositeSensorController::setInfraredRegistrationOffsetY(std::int16_t value) {
    return writeRegister(CompositeSensorRegister::InfraredRegistrationOffsetY,
                         static_cast<std::uint16_t>(value));
}

Result<void> CompositeSensorController::setLowlightRegistrationZoom(std::uint16_t value) {
    if (value > 10) return Result<void>::error(ErrorCode::InvalidArgument, "lowlight registration zoom must be 0..10");
    return writeRegister(CompositeSensorRegister::LowlightRegistrationZoom, value);
}

Result<void> CompositeSensorController::setLowlightRegistrationOffsetX(std::int16_t value) {
    return writeRegister(CompositeSensorRegister::LowlightRegistrationOffsetX,
                         static_cast<std::uint16_t>(value));
}

Result<void> CompositeSensorController::setLowlightRegistrationOffsetY(std::int16_t value) {
    return writeRegister(CompositeSensorRegister::LowlightRegistrationOffsetY,
                         static_cast<std::uint16_t>(value));
}

Result<void> CompositeSensorController::saveCurrentConfig() {
    if (static_cast<std::uint16_t>(CompositeSensorRegister::SaveConfig) == 0xFFFF) {
        return Result<void>::error(ErrorCode::Unsupported,
                                   "save config register address is not defined in current protocol excerpt");
    }
    return writeRegister(CompositeSensorRegister::SaveConfig,
                         static_cast<std::uint16_t>(SaveConfigAction::SaveCurrent));
}

Result<void> CompositeSensorController::restoreDefaultConfig() {
    if (static_cast<std::uint16_t>(CompositeSensorRegister::SaveConfig) == 0xFFFF) {
        return Result<void>::error(ErrorCode::Unsupported,
                                   "save/restore register address is not defined in current protocol excerpt");
    }
    return writeRegister(CompositeSensorRegister::SaveConfig,
                         static_cast<std::uint16_t>(SaveConfigAction::RestoreDefault));
}

Result<void> CompositeSensorController::ensureReady() const {
    if (!isReady()) {
        return Result<void>::error(ErrorCode::NotInitialized, "composite sensor controller is not ready");
    }
    return Result<void>::success();
}

Result<void> CompositeSensorController::sendWriteCommandWithRetry(const std::vector<std::uint8_t>& command) {
    const int attempts = std::max(1, config_.retryCount + 1);
    Status last = Status::success();

    for (int i = 0; i < attempts; ++i) {
        auto ret = sendWriteOnce(command);
        if (ret) return ret;
        last = ret.status();
        TRI_LOG_WARN(LogCategory::Serial) << "composite sensor write command attempt "
                                          << (i + 1) << "/" << attempts
                                          << " failed: " << last.describe();
    }

    return Result<void>::error(ErrorCode::CompositeModeSwitchFailed,
                               "composite sensor write command failed after retries: " + last.describe());
}

Result<CompositeSensorAck>
CompositeSensorController::sendReadCommandWithRetry(const std::vector<std::uint8_t>& command) {
    const int attempts = std::max(1, config_.retryCount + 1);
    Status last = Status::success();

    for (int i = 0; i < attempts; ++i) {
        auto ret = sendReadOnce(command);
        if (ret) return ret;
        last = ret.status();
        TRI_LOG_WARN(LogCategory::Serial) << "composite sensor read command attempt "
                                          << (i + 1) << "/" << attempts
                                          << " failed: " << last.describe();
    }

    return Result<CompositeSensorAck>::error(ErrorCode::CompositeModeSwitchFailed,
                                             "composite sensor read command failed after retries: " + last.describe());
}

Result<void> CompositeSensorController::sendWriteOnce(const std::vector<std::uint8_t>& command) {
    auto writeRet = serial_.writeBytes(command);
    if (!writeRet) return writeRet;

    auto readRet = readExact(kWriteAckBytes, config_.timeoutMs);
    if (!readRet) {
        if (!ackRequired_ && readRet.status().code() == ErrorCode::SerialReadTimeout) {
            TRI_LOG_WARN(LogCategory::Serial)
                << "no composite sensor write ACK received; command write was successful and ACK is optional";
            return Result<void>::success();
        }
        return Result<void>::error(readRet.status().code(), readRet.status().describe());
    }

    auto ackRet = parser_.parseWriteRegisterAck(readRet.value());
    if (!ackRet) {
        return Result<void>::error(ackRet.status().code(), ackRet.status().describe());
    }
    return Result<void>::success();
}

Result<CompositeSensorAck>
CompositeSensorController::sendReadOnce(const std::vector<std::uint8_t>& command) {
    auto writeRet = serial_.writeBytes(command);
    if (!writeRet) {
        return Result<CompositeSensorAck>::error(writeRet.status().code(), writeRet.status().describe());
    }

    auto readRet = readExact(kReadAckBytes, config_.timeoutMs);
    if (!readRet) {
        return Result<CompositeSensorAck>::error(readRet.status().code(), readRet.status().describe());
    }

    return parser_.parseReadRegisterAck(readRet.value());
}

Result<std::vector<std::uint8_t>> CompositeSensorController::readExact(std::size_t expectedBytes,
                                                                       int timeoutMs) {
    std::vector<std::uint8_t> out;
    out.reserve(expectedBytes);

    while (out.size() < expectedBytes) {
        auto chunkRet = serial_.readBytes(expectedBytes - out.size(), timeoutMs);
        if (!chunkRet) {
            return Result<std::vector<std::uint8_t>>::error(chunkRet.status().code(), chunkRet.status().describe());
        }
        const auto& chunk = chunkRet.value();
        if (chunk.empty()) {
            return Result<std::vector<std::uint8_t>>::error(ErrorCode::SerialReadTimeout,
                                                            "serial read returned empty data");
        }
        out.insert(out.end(), chunk.begin(), chunk.end());
    }

    return Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

Result<void> CompositeSensorController::validatePercent(std::uint16_t value, const char* name) const {
    if (value > 100) {
        return Result<void>::error(ErrorCode::InvalidArgument, std::string(name) + " must be 0..100");
    }
    return Result<void>::success();
}

void CompositeSensorController::recordError(const Status& status) {
    ++state_.errorCount;
    state_.lastError = status.describe();
    if (status.code() == ErrorCode::SerialOpenFailed ||
        status.code() == ErrorCode::SerialReadTimeout ||
        status.code() == ErrorCode::SerialWriteFailed ||
        status.code() == ErrorCode::IoError) {
        state_.serialOnline = false;
    }
}

void CompositeSensorController::publishModeChanged(CompositeSensorOutputMode mode) {
    if (!eventBus_) return;

    Event event;
    event.type = EventType::ModeChanged;
    event.name = "CompositeSensorOutputModeChanged";
    event.fields["component"] = "CompositeSensorController";
    event.fields["composite_output_mode"] = toString(mode);
    eventBus_->publish(event);
}

} // namespace tri::device_control
