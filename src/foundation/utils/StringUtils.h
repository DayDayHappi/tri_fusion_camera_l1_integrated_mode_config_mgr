#pragma once

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace tri::foundation::str {

inline std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

inline bool startsWith(const std::string& s, const std::string& prefix) { return s.rfind(prefix, 0) == 0; }
inline bool endsWith(const std::string& s, const std::string& suffix) { return s.size() >= suffix.size() && s.compare(s.size()-suffix.size(), suffix.size(), suffix) == 0; }

inline std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delimiter)) out.push_back(item);
    return out;
}

inline std::string unquote(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

inline bool toBool(std::string s, bool fallback = false) {
    s = trim(std::move(s));
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
    if (s == "false" || s == "0" || s == "no" || s == "off") return false;
    return fallback;
}

} // namespace tri::foundation::str
