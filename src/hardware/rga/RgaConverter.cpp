#include "hardware/rga/RgaConverter.h"

#include <string>
#include <cstring>

#include <im2d.h>
#include <rga.h>

namespace tri::hardware::rga {

using tri::foundation::ErrorCode;
using tri::foundation::Result;

Result<void> RgaConverter::convert(const RgaImageDesc& src, const RgaImageDesc& dst) const {
    if (!src.virAddr || !dst.virAddr) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "RGA convert requires non-null virtual address");
    }

    if (src.width == 0 || src.height == 0 || dst.width == 0 || dst.height == 0) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "RGA convert invalid image size");
    }

    const int srcFmt = toRgaFormat(src.format);
    const int dstFmt = toRgaFormat(dst.format);

    if (srcFmt == 0 || dstFmt == 0) {
        return Result<void>::error(ErrorCode::Unsupported,
                                   "RGA convert unsupported pixel format");
    }

    const int srcHorStride = static_cast<int>(src.horStride ? src.horStride : src.width);
    const int srcVerStride = static_cast<int>(src.verStride ? src.verStride : src.height);
    const int dstHorStride = static_cast<int>(dst.horStride ? dst.horStride : dst.width);
    const int dstVerStride = static_cast<int>(dst.verStride ? dst.verStride : dst.height);

    rga_buffer_t srcBuf = wrapbuffer_virtualaddr(
        src.virAddr,
        static_cast<int>(src.width),
        static_cast<int>(src.height),
        srcFmt,
        srcHorStride,
        srcVerStride
    );

    rga_buffer_t dstBuf = wrapbuffer_virtualaddr(
        dst.virAddr,
        static_cast<int>(dst.width),
        static_cast<int>(dst.height),
        dstFmt,
        dstHorStride,
        dstVerStride
    );

    // 不再调用 imcheck(srcBuf, dstBuf, {}, {})。
    // 当前 RK3588 SDK 的 imcheck 宏对空可变参数不兼容，会触发 zero-size array。
    // 这里直接调用 imcvtcolor，让 RGA SDK 返回真实状态。
    IM_STATUS status = imcvtcolor(srcBuf, dstBuf, srcFmt, dstFmt);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        return Result<void>::error(ErrorCode::IoError,
                                   std::string("RGA imcvtcolor failed: ") + imStrError(status));
    }

    return Result<void>::success();
}

} // namespace tri::hardware::rga
