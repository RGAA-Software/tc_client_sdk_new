//
// Created by RGAA on 2024/1/25.
//

#ifndef TC_CLIENT_ANDROID_SDK_MESSAGES_H
#define TC_CLIENT_ANDROID_SDK_MESSAGES_H

#include <string>
#include <memory>

#include "sdk_errors.h"
#include "tc_message.pb.h"

namespace tc
{

    const std::string kRoomTypeMedia = "media";
    const std::string kRoomTypeFileTransfer = "file_transfer";

    class RawImage;

    // monitor info
    // mon_right_ - mon_left_ != frame_width_ ; cuz awareness settings in Renderer
    class SdkCaptureMonitorInfo {
    public:
        std::string mon_name_;
        int mon_index_ = -1;
        int mon_left_ = 0;
        int mon_top_ = 0;
        int mon_right_ = 0;
        int mon_bottom_ = 0;
        int frame_width_ = -1;
        int frame_height_ = -1;
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

    //
    class SdkMsgTimer16 {
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

    class SdkMsgClipboardReqBuffer {
    public:
        tc::ClipboardReqBuffer req_buffer_;
    };

    // room prepared in relay/p2p mode
    class SdkMsgRoomPrepared {
    public:
        std::string room_type_;
    };

    // room destroyed in relay/p2p mode
    class SdkMsgRoomDestroyed {
    public:

    };

    // remote answer sdp
    class SdkMsgRemoteAnswerSdp {
    public:
        SigAnswerSdpMessage answer_sdp_;
    };

    // remote ice
    class SdkMsgRemoteIce {
    public:
        SigIceMessage ice_;
    };

    // sdk statistics
    class SdkMsgStatistics {
    public:
        // network type
        ClientNetworkType net_type_;
        // total media bytes
        int64_t recv_total_media_bytes_ = 0;
        // receiving media speed
        int64_t recv_media_speed_ = 0;
        //
        int32_t recv_video_fps_ = 0;
        //
        int32_t decode_fps_ = 0;
        //
        int32_t render_fps_ = 0;
    };
}

#endif //TC_CLIENT_ANDROID_SDK_MESSAGES_H
