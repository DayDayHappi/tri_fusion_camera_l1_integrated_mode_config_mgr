#pragma once
#include "foundation/error/Result.h"
namespace tri::hardware::net { class TcpSocket final { public: foundation::Result<void> openSocket(){ return foundation::Result<void>::error(foundation::ErrorCode::Unsupported,"TCP socket wrapper not implemented in L1 phase 1"); } }; }
