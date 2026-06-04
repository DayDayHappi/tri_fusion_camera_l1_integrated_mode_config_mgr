#pragma once

#include <any>
#include <string>
#include <unordered_map>
#include "foundation/event/EventTypes.h"
#include "foundation/time/Timestamp.h"

namespace tri::foundation {

struct Event {
    EventType type{EventType::Unknown};
    std::string name;
    Timestamp timestamp{Timestamp::now()};
    std::unordered_map<std::string, std::string> fields;
    std::any payload;
};

} // namespace tri::foundation
