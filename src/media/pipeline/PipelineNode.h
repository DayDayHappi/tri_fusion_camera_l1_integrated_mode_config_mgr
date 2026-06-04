#pragma once

#include <string>
#include "foundation/error/Result.h"
#include "media/pipeline/PipelineContext.h"

namespace tri::media {

class PipelineNode {
public:
    virtual ~PipelineNode() = default;
    virtual const char* name() const noexcept = 0;
    virtual foundation::Result<void> init(PipelineContext& context) = 0;
    virtual foundation::Result<void> start() = 0;
    virtual void stop() = 0;
};

} // namespace tri::media
