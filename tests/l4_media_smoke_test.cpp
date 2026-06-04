#include "media/frame/FrameQueue.h"
#include "media/frame/FrameSync.h"
#include "media/stream/MainStream.h"
#include "media/process/FormatConvertNode.h"

#include <iostream>

int main() {
    tri::media::FrameQueue queue(2);
    tri::media::VideoFrame f1; f1.sequence = 1; f1.ptsMs = 100;
    tri::media::VideoFrame f2; f2.sequence = 2; f2.ptsMs = 120;
    tri::media::VideoFrame f3; f3.sequence = 3; f3.ptsMs = 140;
    queue.push(f1);
    queue.push(f2);
    queue.push(f3);
    if (queue.dropped() != 1) {
        std::cerr << "FrameQueue drop policy failed\n";
        return 1;
    }

    tri::media::FrameSync sync(30);
    tri::media::VideoFrame visible; visible.sequence = 10; visible.ptsMs = 1000;
    tri::media::VideoFrame composite; composite.sequence = 11; composite.ptsMs = 1018;
    sync.pushVisible(visible);
    sync.pushComposite(composite);
    auto group = sync.trySync();
    if (!group || group->deltaMs != 18) {
        std::cerr << "FrameSync failed\n";
        return 1;
    }


    tri::media::VideoFrame yuyv;
    yuyv.width = 2;
    yuyv.height = 2;
    yuyv.format = tri::media::VideoPixelFormat::Yuyv422;
    yuyv.data = {
        10, 100, 20, 150,
        30, 102, 40, 152,
    };
    tri::media::FormatConvertNode converter(tri::media::VideoPixelFormat::Nv12);
    auto nv12 = converter.process(yuyv);
    if (!nv12 || nv12.value().format != tri::media::VideoPixelFormat::Nv12 || nv12.value().data.size() != 6) {
        std::cerr << "YUYV422 to NV12 conversion failed\n";
        return 1;
    }

    tri::foundation::MediaConfig cfg;
    cfg.codec = "h264";
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps = 25;
    tri::media::MainStream stream;
    auto init = stream.init(cfg);
    if (!init) {
        std::cerr << init.status().describe() << "\n";
        return 1;
    }
    stream.start();
    tri::media::EncodedFrame encoded;
    encoded.sequence = 1;
    encoded.ptsMs = 100;
    encoded.data = {0x01, 0x02, 0x03};
    auto push = stream.push(encoded);
    if (!push) {
        std::cerr << push.status().describe() << "\n";
        return 1;
    }
    auto out = stream.waitFrame(10);
    if (!out || out->data.size() != 3) {
        std::cerr << "MainStream failed\n";
        return 1;
    }

    std::cout << "L4 media smoke test passed\n";
    return 0;
}
