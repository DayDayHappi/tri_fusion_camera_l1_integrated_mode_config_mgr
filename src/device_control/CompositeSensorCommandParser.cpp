#include "device_control/CompositeSensorCommandParser.h"
#include "foundation/error/ErrorCode.h"
#include "foundation/utils/ByteUtils.h"
#include "foundation/utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace tri::device_control {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

namespace {
constexpr std::uint16_t kHeader = 0x55AA;
constexpr std::uint16_t kWriteAckLength = 0x0006;
constexpr std::uint16_t kReadAckLength = 0x0008;
constexpr std::size_t kWriteAckBytes = 10;
constexpr std::size_t kReadAckBytes = 12;

std::string hexWord(std::uint16_t value) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << value;
    return oss.str();
}

std::string asciiFromBytes(const std::vector<std::uint8_t>& bytes) {
    std::string s;
    s.reserve(bytes.size());
    for (auto b : bytes) {
        if (b >= 0x20 && b <= 0x7e) s.push_back(static_cast<char>(b));
    }
    return s;
}
} // namespace

Result<CompositeSensorAck>
CompositeSensorCommandParser::parseAck(const std::vector<std::uint8_t>& bytes,
                                       CompositeSensorOutputMode expectedMode) const {
    auto ackRet = parseWriteRegisterAck(bytes);
    if (!ackRet) return ackRet;

    auto ack = ackRet.value();
    ack.mode = expectedMode;
    return Result<CompositeSensorAck>::ok(std::move(ack));
}

Result<CompositeSensorAck>
CompositeSensorCommandParser::parseWriteRegisterAck(const std::vector<std::uint8_t>& bytes) const {
    if (bytes.size() != kWriteAckBytes) {
        return Result<CompositeSensorAck>::error(
            ErrorCode::SerialAckInvalid,
            "write-register ACK must be 10 bytes, actual=" + std::to_string(bytes.size()) +
            ", raw=" + tri::foundation::bytes::toHex(bytes));
    }

    CompositeSensorAck ack;
    ack.type = CompositeSensorAckType::Ack;
    ack.originalCommand = CompositeSensorCommandCode::WriteRegister;

    const auto header = be16(bytes, 0);
    const auto length = be16(bytes, 2);
    const auto original = be16(bytes, 4);
    const auto status = be16(bytes, 6);
    const auto checksum = be16(bytes, 8);
    const auto expectedChecksum = checksumWords(length, original, status);

    ack.status = static_cast<CompositeSensorStatusWord>(status);
    ack.checksum = checksum;
    ack.expectedChecksum = expectedChecksum;

    if (header != kHeader) {
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid,
                                                "invalid ACK header: " + hexWord(header));
    }
    if (length != kWriteAckLength) {
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid,
                                                "invalid write ACK length: " + hexWord(length));
    }
    if (original != static_cast<std::uint16_t>(CompositeSensorCommandCode::WriteRegister)) {
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid,
                                                "invalid write ACK original command: " + hexWord(original));
    }
    if (status != static_cast<std::uint16_t>(CompositeSensorStatusWord::Executable) &&
        status != static_cast<std::uint16_t>(CompositeSensorStatusWord::Rejected)) {
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid,
                                                "invalid write ACK status word: " + hexWord(status));
    }
    if (checksum != expectedChecksum) {
        return Result<CompositeSensorAck>::error(
            ErrorCode::SerialAckInvalid,
            "invalid write ACK checksum, expected=" + hexWord(expectedChecksum) +
            ", actual=" + hexWord(checksum));
    }
    if (!isExecutable(ack.status)) {
        ack.type = CompositeSensorAckType::Nack;
        ack.message = "write-register command rejected by device, status=" + toString(ack.status);
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid, ack.message);
    }

    ack.message = "write-register ACK OK";
    return Result<CompositeSensorAck>::ok(std::move(ack));
}

Result<CompositeSensorAck>
CompositeSensorCommandParser::parseReadRegisterAck(const std::vector<std::uint8_t>& bytes) const {
    if (bytes.size() != kReadAckBytes) {
        return Result<CompositeSensorAck>::error(
            ErrorCode::SerialAckInvalid,
            "read-register ACK must be 12 bytes, actual=" + std::to_string(bytes.size()) +
            ", raw=" + tri::foundation::bytes::toHex(bytes));
    }

    CompositeSensorAck ack;
    ack.type = CompositeSensorAckType::Ack;
    ack.originalCommand = CompositeSensorCommandCode::ReadRegister;

    const auto header = be16(bytes, 0);
    const auto length = be16(bytes, 2);
    const auto original = be16(bytes, 4);
    const auto status = be16(bytes, 6);
    const auto data = be16(bytes, 8);
    const auto checksum = be16(bytes, 10);
    const auto expectedChecksum = checksumWords(length, original, status, data);

    ack.status = static_cast<CompositeSensorStatusWord>(status);
    ack.data = data;
    ack.checksum = checksum;
    ack.expectedChecksum = expectedChecksum;

    if (header != kHeader) {
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid,
                                                "invalid read ACK header: " + hexWord(header));
    }
    if (length != kReadAckLength) {
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid,
                                                "invalid read ACK length: " + hexWord(length));
    }
    if (original != static_cast<std::uint16_t>(CompositeSensorCommandCode::ReadRegister)) {
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid,
                                                "invalid read ACK original command: " + hexWord(original));
    }
    if (status != static_cast<std::uint16_t>(CompositeSensorStatusWord::Executable) &&
        status != static_cast<std::uint16_t>(CompositeSensorStatusWord::Rejected)) {
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid,
                                                "invalid read ACK status word: " + hexWord(status));
    }
    if (checksum != expectedChecksum) {
        return Result<CompositeSensorAck>::error(
            ErrorCode::SerialAckInvalid,
            "invalid read ACK checksum, expected=" + hexWord(expectedChecksum) +
            ", actual=" + hexWord(checksum));
    }
    if (!isExecutable(ack.status)) {
        ack.type = CompositeSensorAckType::Nack;
        ack.message = "read-register command rejected by device, status=" + toString(ack.status);
        return Result<CompositeSensorAck>::error(ErrorCode::SerialAckInvalid, ack.message);
    }

    ack.message = "read-register ACK OK";
    return Result<CompositeSensorAck>::ok(std::move(ack));
}

Result<CompositeSensorOutputMode>
CompositeSensorCommandParser::parseModeReport(const std::vector<std::uint8_t>& bytes) const {
    auto ackRet = parseReadRegisterAck(bytes);
    if (ackRet) {
        auto mode = outputModeFromRegisterValue(ackRet.value().data);
        if (mode == CompositeSensorOutputMode::Unknown) {
            return Result<CompositeSensorOutputMode>::error(ErrorCode::SerialAckInvalid,
                                                            "unknown fusion mode register value: " +
                                                            std::to_string(ackRet.value().data));
        }
        return Result<CompositeSensorOutputMode>::ok(mode);
    }

    const auto ascii = asciiFromBytes(bytes);
    auto mode = parseModeText(ascii);
    if (mode != CompositeSensorOutputMode::Unknown) {
        return Result<CompositeSensorOutputMode>::ok(mode);
    }

    return Result<CompositeSensorOutputMode>::error(ErrorCode::SerialAckInvalid,
                                                    "unable to parse composite sensor mode report: " +
                                                    ackRet.status().describe());
}

CompositeSensorOutputMode CompositeSensorCommandParser::parseModeText(const std::string& text) {
    return compositeSensorOutputModeFromString(tri::foundation::str::trim(text));
}

std::uint16_t CompositeSensorCommandParser::checksumWords(std::uint16_t a,
                                                          std::uint16_t b,
                                                          std::uint16_t c) {
    return static_cast<std::uint16_t>(a + b + c);
}

std::uint16_t CompositeSensorCommandParser::checksumWords(std::uint16_t a,
                                                          std::uint16_t b,
                                                          std::uint16_t c,
                                                          std::uint16_t d) {
    return static_cast<std::uint16_t>(a + b + c + d);
}

std::uint16_t CompositeSensorCommandParser::be16(const std::vector<std::uint8_t>& bytes,
                                                 std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) |
                                      static_cast<std::uint16_t>(bytes[offset + 1]));
}

} // namespace tri::device_control
