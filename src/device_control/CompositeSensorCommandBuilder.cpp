#include "device_control/CompositeSensorCommandBuilder.h"
#include "foundation/error/ErrorCode.h"
#include "foundation/utils/StringUtils.h"

#include <algorithm>
#include <cctype>

namespace tri::device_control {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

namespace {
constexpr std::uint16_t kHeader = 0x55AA;
constexpr std::uint16_t kReadLength = 0x0006;
constexpr std::uint16_t kWriteLength = 0x0008;
}

Result<void> CompositeSensorCommandBuilder::init(const foundation::SerialConfig& config) {
    if (!config.enable) {
        return Result<void>::error(ErrorCode::ConfigError, "composite sensor serial config is disabled");
    }
    config_ = &config;
    return Result<void>::success();
}

Result<std::vector<std::uint8_t>>
CompositeSensorCommandBuilder::buildSetOutputMode(CompositeSensorOutputMode mode) const {
    const auto value = outputModeRegisterValue(mode);
    if (value == 0) {
        return Result<std::vector<std::uint8_t>>::error(ErrorCode::InvalidArgument,
                                                        "unknown composite sensor output mode");
    }
    return buildWriteRegister(CompositeSensorRegister::FusionMode, value);
}

Result<std::vector<std::uint8_t>> CompositeSensorCommandBuilder::buildQueryOutputMode() const {
    return buildReadRegister(CompositeSensorRegister::FusionMode);
}

Result<std::vector<std::uint8_t>>
CompositeSensorCommandBuilder::buildReadRegister(CompositeSensorRegister reg) const {
    return buildReadRegister(static_cast<std::uint16_t>(reg));
}

Result<std::vector<std::uint8_t>>
CompositeSensorCommandBuilder::buildWriteRegister(CompositeSensorRegister reg,
                                                  std::uint16_t value) const {
    return buildWriteRegister(static_cast<std::uint16_t>(reg), value);
}

Result<std::vector<std::uint8_t>>
CompositeSensorCommandBuilder::buildReadRegister(std::uint16_t address) const {
    if (config_ == nullptr) {
        return Result<std::vector<std::uint8_t>>::error(ErrorCode::NotInitialized,
                                                        "composite sensor command builder is not initialized");
    }

    std::vector<std::uint8_t> out;
    out.reserve(10);
    appendBe16(out, kHeader);
    appendBe16(out, kReadLength);
    appendBe16(out, static_cast<std::uint16_t>(CompositeSensorCommandCode::ReadRegister));
    appendBe16(out, address);
    appendBe16(out, checksumWords(kReadLength,
                                  static_cast<std::uint16_t>(CompositeSensorCommandCode::ReadRegister),
                                  address));
    return Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

Result<std::vector<std::uint8_t>>
CompositeSensorCommandBuilder::buildWriteRegister(std::uint16_t address, std::uint16_t value) const {
    if (config_ == nullptr) {
        return Result<std::vector<std::uint8_t>>::error(ErrorCode::NotInitialized,
                                                        "composite sensor command builder is not initialized");
    }

    std::vector<std::uint8_t> out;
    out.reserve(12);
    appendBe16(out, kHeader);
    appendBe16(out, kWriteLength);
    appendBe16(out, static_cast<std::uint16_t>(CompositeSensorCommandCode::WriteRegister));
    appendBe16(out, address);
    appendBe16(out, value);
    appendBe16(out, checksumWords(kWriteLength,
                                  static_cast<std::uint16_t>(CompositeSensorCommandCode::WriteRegister),
                                  address,
                                  value));
    return Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

Result<std::vector<std::uint8_t>>
CompositeSensorCommandBuilder::commandByKey(const std::string& key) const {
    if (config_ == nullptr) {
        return Result<std::vector<std::uint8_t>>::error(ErrorCode::NotInitialized,
                                                        "composite sensor command builder is not initialized");
    }
    auto it = config_->commands.find(key);
    if (it == config_->commands.end() || tri::foundation::str::trim(it->second).empty()) {
        return Result<std::vector<std::uint8_t>>::error(ErrorCode::ConfigError,
                                                        "serial command is empty: " + key);
    }
    return parseHexCommand(it->second);
}

Result<std::vector<std::uint8_t>>
CompositeSensorCommandBuilder::parseHexCommand(const std::string& text) {
    std::string normalized;
    normalized.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);

        if (text[i] == '0' && i + 1 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
            ++i;
            continue;
        }

        if (std::isxdigit(ch)) {
            normalized.push_back(static_cast<char>(text[i]));
            continue;
        }

        if (std::isspace(ch) || text[i] == ',' || text[i] == ':' || text[i] == '-' || text[i] == '_') {
            continue;
        }

        return Result<std::vector<std::uint8_t>>::error(ErrorCode::ParseError,
                                                        "invalid hex command character");
    }

    if (normalized.empty()) {
        return Result<std::vector<std::uint8_t>>::error(ErrorCode::InvalidArgument,
                                                        "hex command is empty");
    }
    if ((normalized.size() % 2) != 0) {
        return Result<std::vector<std::uint8_t>>::error(ErrorCode::ParseError,
                                                        "hex command must contain even number of digits");
    }

    std::vector<std::uint8_t> out;
    out.reserve(normalized.size() / 2);
    for (std::size_t i = 0; i < normalized.size(); i += 2) {
        const auto byteText = normalized.substr(i, 2);
        try {
            auto v = std::stoul(byteText, nullptr, 16);
            out.push_back(static_cast<std::uint8_t>(v & 0xff));
        } catch (...) {
            return Result<std::vector<std::uint8_t>>::error(ErrorCode::ParseError,
                                                            "failed to parse hex byte");
        }
    }
    return Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

std::uint16_t CompositeSensorCommandBuilder::checksumWords(std::uint16_t a,
                                                           std::uint16_t b,
                                                           std::uint16_t c) {
    return static_cast<std::uint16_t>(a + b + c);
}

std::uint16_t CompositeSensorCommandBuilder::checksumWords(std::uint16_t a,
                                                           std::uint16_t b,
                                                           std::uint16_t c,
                                                           std::uint16_t d) {
    return static_cast<std::uint16_t>(a + b + c + d);
}

void CompositeSensorCommandBuilder::appendBe16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
}

} // namespace tri::device_control
