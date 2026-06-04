#pragma once
#include "protocol/private/PrivateProtocolTypes.h"
#include <string>
namespace tri::protocol::private_api {
class PrivateProtocolParser final {
public:
    static PrivateRequest parseLine(const std::string& line);
    static std::string getParam(const PrivateRequest& req, const std::string& key, const std::string& fallback = {});
};
} // namespace tri::protocol::private_api
