#pragma once

#include <sys/types.h>

#include <cstdint>
#include <string>
#include <vector>

namespace tri::media::gstreamer {

class GStreamerProcess {
public:
    GStreamerProcess() = default;
    ~GStreamerProcess();

    GStreamerProcess(const GStreamerProcess&) = delete;
    GStreamerProcess& operator=(const GStreamerProcess&) = delete;

    bool start(const std::vector<std::string>& args);
    bool stop(std::int32_t timeoutMs);
    bool isRunning() const;

    pid_t pid() const;
    int exitCode() const;
    const std::string& lastError() const;

private:
    bool waitForExit(std::int32_t timeoutMs) const;
    static std::vector<char*> makeArgv(std::vector<std::string>& args);

private:
    mutable pid_t pid_{-1};
    mutable int exitCode_{-1};
    std::string lastError_;
};

} // namespace tri::media::gstreamer
