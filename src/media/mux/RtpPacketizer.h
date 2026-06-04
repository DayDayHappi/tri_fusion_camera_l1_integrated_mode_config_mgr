#pragma once
#include <vector>
#include "media/mux/MuxTypes.h"
namespace tri::media { class RtpPacketizer final { public: std::vector<MuxPacket> packetize(const MuxPacket& packet) { return {packet}; } }; }
