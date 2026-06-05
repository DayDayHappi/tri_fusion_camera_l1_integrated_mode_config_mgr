#include "media/gstreamer/GStreamerProcess.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <iostream>
namespace tri::media::gstreamer {

GStreamerProcess::~GStreamerProcess() {
    if (isRunning()) {
        stop(1000);
    }
}

bool GStreamerProcess::start(const std::vector<std::string>& argsIn) {
    std::cerr << "[VIDEO][PROCESS] start request received, argc="
              << argsIn.size() << "\n";
    if (argsIn.empty()) {
        lastError_ = "empty gstreamer argument list";
        return false;
    }

    if (isRunning()) {
        lastError_ = "gstreamer process already running";
        return false;
    }

    std::vector<std::string> args = argsIn;
    auto argv = makeArgv(args);
    std::cerr << "[VIDEO][PROCESS] fork gst-launch process\n";
    const pid_t child = ::fork();
    if (child < 0) {
        lastError_ = std::string("fork failed: ") + std::strerror(errno);
        return false;
    }

    if (child == 0) {
        ::setpgid(0, 0);
        ::execvp(argv[0], argv.data());
        ::_exit(127);
    }

    pid_ = child;
    exitCode_ = -1;
    lastError_.clear();
    std::cerr << "[VIDEO][PROCESS] gst-launch forked, pid="
          << pid_ << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!isRunning()) {
        lastError_ = "gst-launch exited immediately";
        std::cerr << "[VIDEO][PROCESS] gst-launch exited immediately, exit_code="
              << exitCode_ << "\n";
        return false;
    }
std::cerr << "[VIDEO][PROCESS] gst-launch is running, pid="
          << pid_ << "\n";
    return true;
}

bool GStreamerProcess::stop(std::int32_t timeoutMs) {
    if (pid_ <= 0) {
        std::cerr << "[VIDEO][PROCESS] stop skipped, no running gst-launch pid\n";
        return true;
    }

    if (!isRunning()) {
        pid_ = -1;
        return true;
    }

    // gst-launch -e handles SIGINT similarly to Ctrl+C and tries to send EOS.
    std::cerr << "[VIDEO][PROCESS] send SIGINT to gst-launch pid="
          << pid_ << "\n";
    ::kill(-pid_, SIGINT);
    if (waitForExit(timeoutMs)) {
        pid_ = -1;
        return true;
    }
std::cerr << "[VIDEO][PROCESS] SIGINT timeout, send SIGTERM to gst-launch pid="
          << pid_ << "\n";
    ::kill(-pid_, SIGTERM);
    if (waitForExit(500)) {
        pid_ = -1;
        return true;
    }
std::cerr << "[VIDEO][PROCESS] SIGTERM timeout, send SIGKILL to gst-launch pid="
          << pid_ << "\n";
    ::kill(-pid_, SIGKILL);
    if (waitForExit(500)) {
        pid_ = -1;
        return true;
    }

    lastError_ = "failed to stop gst-launch process";
    return false;
}

bool GStreamerProcess::isRunning() const {
    if (pid_ <= 0) {
        return false;
    }

    int status = 0;
    const pid_t ret = ::waitpid(pid_, &status, WNOHANG);
    if (ret == 0) {
        return true;
    }

    if (ret == pid_) {
        if (WIFEXITED(status)) {
            exitCode_ = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exitCode_ = 128 + WTERMSIG(status);
        }
        pid_ = -1;
        return false;
    }

    if (ret < 0 && errno == ECHILD) {
        pid_ = -1;
        return false;
    }

    return true;
}

pid_t GStreamerProcess::pid() const {
    return pid_;
}

int GStreamerProcess::exitCode() const {
    return exitCode_;
}

const std::string& GStreamerProcess::lastError() const {
    return lastError_;
}

bool GStreamerProcess::waitForExit(std::int32_t timeoutMs) const {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!isRunning()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return !isRunning();
}

std::vector<char*> GStreamerProcess::makeArgv(std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

} // namespace tri::media::gstreamer
