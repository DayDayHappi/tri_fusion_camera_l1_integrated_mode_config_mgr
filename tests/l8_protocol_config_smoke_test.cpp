#include "protocol/ProtocolConfigLoader.h"
#include "protocol/ProtocolConfigStore.h"
#include "protocol/gb28181/Gb28181Types.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string configDir = argc >= 2 ? argv[1] : "./configs";

    auto loaded = tri::protocol::ProtocolConfigLoader::loadAll(configDir);
    if (!loaded) {
        std::cerr << "[FAIL] load protocol endpoint config: "
                  << loaded.status().describe() << "\n";
        return 1;
    }

    const auto& gb = loaded.value().gb28181;
    std::cout << "[OK] loaded protocol endpoint configs from " << configDir << "\n";
    std::cout << "[GB28181]\n";
    std::cout << "  local_device_id=" << gb.local.deviceId << "\n";
    std::cout << "  local_domain=" << gb.local.domain << "\n";
    std::cout << "  local_ip=" << gb.local.ip << "\n";
    std::cout << "  local_port=" << gb.local.port << "\n";
    std::cout << "  server_id=" << gb.server.id << "\n";
    std::cout << "  server_domain=" << gb.server.domain << "\n";
    std::cout << "  server_ip=" << gb.server.ip << "\n";
    std::cout << "  server_port=" << gb.server.port << "\n";
    std::cout << "  channel_id=" << gb.channel.id << "\n";

    auto profile = tri::protocol::gb28181::makeGbDeviceProfile(gb);
    if (profile.serverIp.empty() || profile.serverPort == 0) {
        std::cerr << "[FAIL] GB28181 profile target server invalid\n";
        return 2;
    }

    tri::protocol::ProtocolConfigStore store;
    auto s = store.load(configDir);
    if (!s) {
        std::cerr << "[FAIL] config store load: " << s.status().describe() << "\n";
        return 3;
    }

    auto fields = store.flattenGb28181();
    std::cout << "[OK] config store ready, fields=" << fields.size() << "\n";
    std::cout << "[PASS] L8 protocol config smoke test passed\n";
    return 0;
}
