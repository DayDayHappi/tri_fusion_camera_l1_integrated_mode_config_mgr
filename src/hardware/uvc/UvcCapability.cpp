#include "hardware/uvc/UvcCapability.h"

namespace tri::hardware::uvc {

std::string toString(UvcNodeKind kind) {
    switch (kind) {
        case UvcNodeKind::VideoCapture: return "video_capture";
        case UvcNodeKind::MetadataCapture: return "metadata_capture";
        case UvcNodeKind::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace tri::hardware::uvc
