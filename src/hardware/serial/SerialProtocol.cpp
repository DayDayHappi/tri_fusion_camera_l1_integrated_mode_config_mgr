#include "hardware/serial/SerialProtocol.h"
#include "foundation/utils/ByteUtils.h"

namespace tri::hardware::serial {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

std::uint16_t SerialProtocol::checksum16(const std::vector<std::uint8_t>& bytes) {
    return tri::foundation::bytes::checksum16(bytes);
}

SerialFrame SerialProtocol::appendChecksumBE(std::vector<std::uint8_t> payload) {
    auto sum = checksum16(payload);
    tri::foundation::bytes::appendBe16(payload, sum);
    return SerialFrame{std::move(payload)};
}

Result<SerialFrame> SerialProtocol::parseWithChecksumBE(const std::vector<std::uint8_t>& frame) {
    if (frame.size() < 3) return Result<SerialFrame>::error(ErrorCode::InvalidArgument, "serial frame too short");
    std::vector<std::uint8_t> payload(frame.begin(), frame.end() - 2);
    const auto expected = tri::foundation::bytes::be16(frame.data() + frame.size() - 2);
    const auto actual = checksum16(payload);
    if (expected != actual) return Result<SerialFrame>::error(ErrorCode::SerialAckInvalid, "serial checksum mismatch");
    return Result<SerialFrame>::ok(SerialFrame{std::move(payload)});
}

} // namespace tri::hardware::serial
