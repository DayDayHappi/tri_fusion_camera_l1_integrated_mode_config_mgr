#pragma once
#include <string>
#include <utility>
namespace tri::protocol::onvif {
struct OnvifDeviceInfo { std::string manufacturer{"TriFusion"}; std::string model{"Tri-Fusion Camera"}; std::string firmware{"0.1"}; std::string serialNumber{"unknown"}; };
class OnvifDeviceService final { public: const OnvifDeviceInfo& info() const noexcept { return info_; } void setInfo(OnvifDeviceInfo info) { info_ = std::move(info); } private: OnvifDeviceInfo info_{}; };
} // namespace tri::protocol::onvif
