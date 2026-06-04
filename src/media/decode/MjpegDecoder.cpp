#include "media/decode/MjpegDecoder.h"
namespace tri::media {
foundation::Result<VideoFrame> MjpegDecoder::decode(const VideoFrame& encoded) {
    // 第一版占位解码器：不伪造硬件解码结果，只把 MJPEG 输入向后传递。
    // 上板接入 RK MPP 后应使用 MppMjpegDecoder 注入真实 MppDecoder。
    return foundation::Result<VideoFrame>::ok(encoded);
}
} // namespace tri::media
