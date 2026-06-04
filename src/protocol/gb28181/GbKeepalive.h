#pragma once
#include "foundation/error/Result.h"
namespace tri::protocol::gb28181 { class GbKeepalive final { public: tri::foundation::Result<void> start(int intervalSeconds = 60); void stop(); bool running() const noexcept { return running_; } private: int intervalSeconds_{60}; bool running_{false}; }; }
