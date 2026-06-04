#include "foundation/utils/FileUtils.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tri::foundation::file {

bool exists(const std::string& path) { return std::filesystem::exists(path); }

Result<std::string> readText(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return Result<std::string>::error(ErrorCode::IoError, "failed to open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Result<void> writeText(const std::string& path, const std::string& text) {
    std::ofstream out(path);
    if (!out.is_open()) return Result<void>::error(ErrorCode::IoError, "failed to write file: " + path);
    out << text;
    return Result<void>::success();
}

Result<std::vector<std::string>> listFiles(const std::string& directory) {
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) return Result<std::vector<std::string>>::error(ErrorCode::IoError, ec.message());
        if (entry.is_regular_file()) files.push_back(entry.path().string());
    }
    return files;
}

} // namespace tri::foundation::file
