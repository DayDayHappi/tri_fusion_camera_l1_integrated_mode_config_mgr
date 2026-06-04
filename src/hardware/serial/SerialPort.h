#pragma once

#include "foundation/error/Result.h"
#include "hardware/serial/SerialTypes.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tri::hardware::serial {

class SerialPort final {
public:
    ~SerialPort();
    SerialPort() = default;
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    foundation::Result<void> openPort(const SerialPortConfig& config);
    void closePort();
    bool isOpen() const noexcept { return fd_ >= 0; }
    const SerialPortConfig& config() const noexcept { return config_; }

    foundation::Result<void> writeBytes(const std::vector<std::uint8_t>& bytes);
    foundation::Result<std::vector<std::uint8_t>> readBytes(std::size_t maxBytes, int timeoutMs);

private:
    foundation::Result<void> configureTermios();
    int fd_{-1};
    SerialPortConfig config_{};
};

} // namespace tri::hardware::serial
