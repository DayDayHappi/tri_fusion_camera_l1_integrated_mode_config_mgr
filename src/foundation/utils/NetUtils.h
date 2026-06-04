#pragma once

#include <string>

namespace tri::foundation::net {

bool isValidPort(int port);
bool isLikelyIpv4(const std::string& ip);

} // namespace tri::foundation::net
