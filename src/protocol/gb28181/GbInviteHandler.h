#pragma once
#include "foundation/error/Result.h"
#include "protocol/gb28181/Gb28181Types.h"
#include "protocol/gb28181/GbRtpSender.h"
namespace tri::protocol::gb28181 { class GbInviteHandler final { public: tri::foundation::Result<void> init(GbRtpSender* sender); tri::foundation::Result<void> handleInvite(GbInviteContext context); void bye(); private: GbRtpSender* sender_{nullptr}; }; }
