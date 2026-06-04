#pragma once
#include "foundation/error/Result.h"
#include "hardware/mpp/MppTypes.h"

namespace tri::hardware::mpp {

class MppDecoder {
public:
    virtual ~MppDecoder() = default;
    virtual foundation::Result<void> init(MppCodec codec) = 0;
    virtual foundation::Result<MppFrame> decode(const MppPacket& packet) = 0;
    virtual void release() = 0;
};

} // namespace tri::hardware::mpp
