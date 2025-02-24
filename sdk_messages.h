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
    class SdkCaptureMonitorInfo {
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
    class SdkMsgTimer1000 {
    };

    //
    class SdkMsgTimer2000 {
    };

    //
    class SdkMsgTimer100 {
    };

    // change monitor resolution
    class SdkMsgChangeMonitorResolutionResult {
    public:
        std::string monitor_name_;
        bool result = false;
    };

    // errors
    class SdkMsgError {
    public:
        SdkErrorCode code_;
        std::string msg_;
    };

    // progress connection begin
    class SdkMsgNetworkPrePing {
    public:

    };

    class SdkMsgNetworkConnected {
    public:

    };

    class SdkMsgNetworkDisConnected {
    public:

    };

    class SdkMsgFirstConfigInfoCallback {
    public:

    };

    class SdkMsgFirstVideoFrameDecoded {
    public:
        std::shared_ptr<RawImage> raw_image_ = nullptr;
        SdkCaptureMonitorInfo mon_info_;
    };
    // progress connection end


}

#endif //TC_CLIENT_ANDROID_SDK_MESSAGES_H
