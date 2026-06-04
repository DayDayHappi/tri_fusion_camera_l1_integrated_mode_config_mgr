#include "hardware/serial/SerialPort.h"
#include "foundation/log/Logger.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace tri::hardware::serial {
using tri::foundation::ErrorCode;
using tri::foundation::Result;

namespace {
speed_t toSpeed(int baudrate) {
    switch (baudrate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default: return 0;
    }
}
}

SerialPort::~SerialPort() { closePort(); }

Result<void> SerialPort::openPort(const SerialPortConfig& config) {
    if (isOpen()) return Result<void>::error(ErrorCode::AlreadyInitialized, "serial port already open");
    if (config.device.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "serial device is empty");
    config_ = config;
    fd_ = ::open(config.device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) return Result<void>::error(ErrorCode::SerialOpenFailed, std::string("open ") + config.device + ": " + std::strerror(errno));
    auto ret = configureTermios();
    if (!ret) { closePort(); return ret; }
    TRI_LOG_INFO(tri::foundation::LogCategory::Serial) << "opened serial " << config.device << " baud=" << config.baudrate;
    return Result<void>::success();
}

void SerialPort::closePort() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
}

Result<void> SerialPort::configureTermios() {
    termios tio{};
    if (::tcgetattr(fd_, &tio) != 0) return Result<void>::error(ErrorCode::IoError, std::string("tcgetattr: ") + std::strerror(errno));
    ::cfmakeraw(&tio);
    speed_t speed = toSpeed(config_.baudrate);
    if (speed == 0) return Result<void>::error(ErrorCode::Unsupported, "unsupported baudrate: " + std::to_string(config_.baudrate));
    ::cfsetispeed(&tio, speed);
    ::cfsetospeed(&tio, speed);

    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~CSIZE;
    switch (config_.dataBits) {
        case 7: tio.c_cflag |= CS7; break;
        case 8: tio.c_cflag |= CS8; break;
        default: return Result<void>::error(ErrorCode::Unsupported, "unsupported data bits: " + std::to_string(config_.dataBits));
    }
    if (config_.stopBits == 2) tio.c_cflag |= CSTOPB;
    else if (config_.stopBits == 1) tio.c_cflag &= ~CSTOPB;
    else return Result<void>::error(ErrorCode::Unsupported, "unsupported stop bits: " + std::to_string(config_.stopBits));

    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~PARODD;
    if (config_.parity == SerialParity::Odd) { tio.c_cflag |= PARENB; tio.c_cflag |= PARODD; }
    if (config_.parity == SerialParity::Even) { tio.c_cflag |= PARENB; }
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (::tcsetattr(fd_, TCSANOW, &tio) != 0) return Result<void>::error(ErrorCode::IoError, std::string("tcsetattr: ") + std::strerror(errno));
    ::tcflush(fd_, TCIOFLUSH);
    return Result<void>::success();
}

Result<void> SerialPort::writeBytes(const std::vector<std::uint8_t>& bytes) {
    if (fd_ < 0) return Result<void>::error(ErrorCode::NotInitialized, "serial port not open");
    if (bytes.empty()) return Result<void>::error(ErrorCode::InvalidArgument, "serial write bytes empty");
    std::size_t written = 0;
    while (written < bytes.size()) {
        auto n = ::write(fd_, bytes.data() + written, bytes.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return Result<void>::error(ErrorCode::SerialWriteFailed, std::string("serial write: ") + std::strerror(errno));
        }
        written += static_cast<std::size_t>(n);
    }
    return Result<void>::success();
}

Result<std::vector<std::uint8_t>> SerialPort::readBytes(std::size_t maxBytes, int timeoutMs) {
    if (fd_ < 0) return Result<std::vector<std::uint8_t>>::error(ErrorCode::NotInitialized, "serial port not open");
    if (maxBytes == 0) return Result<std::vector<std::uint8_t>>::error(ErrorCode::InvalidArgument, "maxBytes is zero");
    pollfd pfd{fd_, POLLIN, 0};
    int pr = ::poll(&pfd, 1, timeoutMs);
    if (pr == 0) return Result<std::vector<std::uint8_t>>::error(ErrorCode::SerialReadTimeout, "serial read timeout");
    if (pr < 0) return Result<std::vector<std::uint8_t>>::error(ErrorCode::IoError, std::string("serial poll: ") + std::strerror(errno));
    std::vector<std::uint8_t> out(maxBytes);
    auto n = ::read(fd_, out.data(), out.size());
    if (n < 0) return Result<std::vector<std::uint8_t>>::error(ErrorCode::IoError, std::string("serial read: ") + std::strerror(errno));
    out.resize(static_cast<std::size_t>(n));
    return Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

} // namespace tri::hardware::serial
