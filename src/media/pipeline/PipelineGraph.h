#pragma once
#include <string>
#include <vector>
namespace tri::media {
class PipelineGraph final {
public:
    void addNode(std::string name) { nodes_.push_back(std::move(name)); }
    const std::vector<std::string>& nodes() const noexcept { return nodes_; }
    void clear() { nodes_.clear(); }
private:
    std::vector<std::string> nodes_;
};
} // namespace tri::media
