#include "protocol/private/PrivateProtocolParser.h"
#include <sstream>
namespace tri::protocol::private_api {
PrivateRequest PrivateProtocolParser::parseLine(const std::string& line) {
    PrivateRequest req;
    std::istringstream iss(line);
    iss >> req.method >> req.path;
    std::string kv;
    while (iss >> kv) {
        const auto pos = kv.find('=');
        if (pos == std::string::npos) continue;
        req.params[kv.substr(0, pos)] = kv.substr(pos + 1);
    }
    return req;
}
std::string PrivateProtocolParser::getParam(const PrivateRequest& req, const std::string& key, const std::string& fallback) {
    auto it = req.params.find(key);
    return it == req.params.end() ? fallback : it->second;
}
} // namespace tri::protocol::private_api
