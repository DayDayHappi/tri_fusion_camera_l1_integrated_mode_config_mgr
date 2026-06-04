#pragma once

#include <functional>
#include "foundation/event/Event.h"

namespace tri::foundation {

using EventCallback = std::function<void(const Event&)>;
using SubscriptionId = std::uint64_t;

} // namespace tri::foundation
