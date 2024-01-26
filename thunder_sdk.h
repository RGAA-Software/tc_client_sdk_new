//
// Created by hy on 2023/12/26.
//

#ifndef TC_CLIENT_PC_THUNDERSDK_H
#define TC_CLIENT_PC_THUNDERSDK_H

#include <iostream>
#include <string>

#include "tc_message.pb.h"

namespace tc
{
    class Data;
    class Thread;
    class WSClient;
    class FFmpegVideoDecoder;
    class RawImage;
    class MessageNotifier;

    // param
    class ThunderSdkParams {
    public:
        [[nodiscard]] std::string MakeReqPath() const;

    public:
        bool ssl_ = false;
        std::string ip_;
        int port_;
        std::string req_path_;
    };

    // callbacks
    using OnVideoFrameDecodedCallback = std::function<void(const std::shared_ptr<RawImage>&)>;

    class ThunderSdk {
    public:

        static std::shared_ptr<ThunderSdk> Make(const std::shared_ptr<MessageNotifier>& notifier);

        ThunderSdk(const std::shared_ptr<MessageNotifier>& notifier);
        ~ThunderSdk();

        bool Init(const ThunderSdkParams& params);
        void Start();
        void Exit();

        void RegisterOnVideoFrameDecodedCallback(OnVideoFrameDecodedCallback&& cbk) { this->video_frame_cbk_ = std::move(cbk); }

    private:

        void SendFirstFrameMessage(const std::shared_ptr<RawImage>& image);

    private:
        std::shared_ptr<MessageNotifier> msg_notifier_ = nullptr;
        ThunderSdkParams sdk_params_;
        std::shared_ptr<WSClient> ws_client_ = nullptr;
        std::shared_ptr<FFmpegVideoDecoder> video_decoder_ = nullptr;

        // callbacks
        OnVideoFrameDecodedCallback video_frame_cbk_;

        bool first_frame_ = false;

        bool exit_ = false;
    };

}

#endif //TC_CLIENT_PC_THUNDERSDK_H
