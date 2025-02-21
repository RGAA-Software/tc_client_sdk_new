//
// Created by RGAA on 2024/1/25.
//

#ifndef TC_CLIENT_ANDROID_SDK_MESSAGES_H
#define TC_CLIENT_ANDROID_SDK_MESSAGES_H

#include <string>
#include <memory>

#include "sdk_errors.h"

namespace tc
{

    class RawImage;

    // monitor info
    // mon_right_ - mon_left_ != frame_width_ ; cuz awareness settings in Renderer
    class CaptureMonitorInfo {
    public:
        std::string mon_name_;
        int mon_left_;
        int mon_top_;
        int mon_right_;
        int mon_bottom_;
        int frame_width_;
        int frame_height_;

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

    // change monitor resolution
    class MsgChangeMonitorResolutionResult {
    public:
        std::string monitor_name_;
        bool result = false;
    };

    // errors
    class MsgSdkError {
    public:
        SdkErrorCode code_;
        std::string msg_;
    };

    // progress connection begin
    class MsgNetworkPrePing {
    public:

    };

    class MsgNetworkConnected {
    public:

    };

    class MsgNetworkDisConnected {
    public:

    };

    class MsgFirstConfigInfoCallback {
    public:

    };

    class MsgFirstVideoFrameDecoded {
    public:
        std::shared_ptr<RawImage> raw_image_ = nullptr;
        CaptureMonitorInfo mon_info_;
    };
    // progress connection end


}

#endif //TC_CLIENT_ANDROID_SDK_MESSAGES_H
