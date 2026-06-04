#pragma once

#include <string>
#include "foundation/error/ErrorCode.h"

namespace tri::foundation {

std::string errorMessage(ErrorCode code);

} // namespace tri::foundation
