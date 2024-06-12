//
// Created by hy on 2024/1/25.
//

#ifndef TC_CLIENT_ANDROID_SDK_MESSAGES_H
#define TC_CLIENT_ANDROID_SDK_MESSAGES_H

#include <string>
#include <memory>

namespace tc
{

    class RawImage;

    // monitor info
    class CaptureMonitorInfo {
    public:
        int mon_idx_;
        std::string mon_name_;
        int mon_left_;
        int mon_top_;
        int mon_right_;
        int mon_bottom_;
    };

    //
    class MsgFirstFrameDecoded {
    public:
        std::shared_ptr<RawImage> raw_image_ = nullptr;
        CaptureMonitorInfo mon_info_;
    };

    //
    class MsgFrameDecoded {
    public:
        uint64_t frame_idx_;
        std::shared_ptr<RawImage> raw_image_ = nullptr;
        CaptureMonitorInfo mon_info_;
    };

    //
    class MsgTimer1000 {
    };

    //
    class MsgTimer2000 {
    };

    //
    class MsgTimer100 {
    };
}

#endif //TC_CLIENT_ANDROID_SDK_MESSAGES_H
