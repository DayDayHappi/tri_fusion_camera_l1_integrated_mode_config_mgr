#pragma once

#include <string>
#include <vector>
#include "foundation/error/Result.h"

namespace tri::foundation::file {

bool exists(const std::string& path);
Result<std::string> readText(const std::string& path);
Result<void> writeText(const std::string& path, const std::string& text);
Result<std::vector<std::string>> listFiles(const std::string& directory);

} // namespace tri::foundation::file
