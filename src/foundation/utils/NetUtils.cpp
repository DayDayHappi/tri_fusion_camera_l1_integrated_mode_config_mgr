#include "foundation/utils/NetUtils.h"
#include <sstream>

namespace tri::foundation::net {

bool isValidPort(int port) { return port > 0 && port <= 65535; }

bool isLikelyIpv4(const std::string& ip) {
    std::stringstream ss(ip);
    std::string part;
    int count = 0;
    while (std::getline(ss, part, '.')) {
        if (part.empty() || part.size() > 3) return false;
        int n = 0;
        for (char c : part) { if (c < '0' || c > '9') return false; n = n * 10 + (c - '0'); }
        if (n < 0 || n > 255) return false;
        ++count;
    }
    return count == 4;
}

} // namespace tri::foundation::net
