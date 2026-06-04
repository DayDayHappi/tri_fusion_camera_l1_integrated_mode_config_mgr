#pragma once

#include <cstdint>
#include <string>

namespace tri::hardware::serial {

enum class SerialParity { None, Odd, Even };

struct SerialPortConfig {
    std::string device;
    int baudrate{115200};
    int dataBits{8};
    int stopBits{1};
    SerialParity parity{SerialParity::None};
    int timeoutMs{500};
};

} // namespace tri::hardware::serial
