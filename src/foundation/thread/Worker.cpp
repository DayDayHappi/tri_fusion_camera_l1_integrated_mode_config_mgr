#include "foundation/thread/Worker.h"
#include "foundation/thread/Thread.h"

namespace tri::foundation {

Worker::~Worker() { stop(); }

Result<void> Worker::start(std::string name, Task task) {
    if (running_) return Result<void>::error(ErrorCode::Busy, "worker already running");
    if (!task) return Result<void>::error(ErrorCode::InvalidArgument, "worker task is empty");
    name_ = std::move(name);
    stopFlag_ = false;
    running_ = true;
    thread_ = std::thread([this, task = std::move(task)]() mutable {
        setCurrentThreadName(name_);
        task(stopFlag_);
        running_ = false;
    });
    return Result<void>::success();
}

void Worker::stop() {
    stopFlag_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
}

bool Worker::running() const noexcept { return running_; }

} // namespace tri::foundation
