//
// Created by hy on 2023/12/26.
//

#include "thunder_sdk.h"

#include "tc_common/log.h"
#include "tc_common/file.h"
#include "tc_common/message_notifier.h"

#include "ws_client.h"
#include "ffmpeg_video_decoder.h"
#include "raw_image.h"
#include "tc_message.pb.h"
#include "sdk_messages.h"

namespace tc
{

    std::string ThunderSdkParams::MakeReqPath() const {
        auto base_url = ip_ + ":" + std::to_string(port_) + req_path_;
        return ssl_ ? "wss://" +  base_url : "ws://" + base_url;
    }

    ///

    std::shared_ptr<ThunderSdk> ThunderSdk::Make(const std::shared_ptr<MessageNotifier>& notifier) {
        return std::make_shared<ThunderSdk>(notifier);
    }

    ThunderSdk::ThunderSdk(const std::shared_ptr<MessageNotifier>& notifier) {
        this->msg_notifier_ = notifier;
    }

    ThunderSdk::~ThunderSdk() {

    }

    bool ThunderSdk::Init(const ThunderSdkParams& params) {
        sdk_params_ = params;

        return true;
    }

    void ThunderSdk::Start() {
        // video decoder
        video_decoder_ = FFmpegVideoDecoder::Make();

        // websocket client
        ws_client_ = WSClient::Make(sdk_params_.MakeReqPath());
        ws_client_->SetOnVideoFrameMsgCallback([=, this](const VideoFrame& frame) {
            bool need_init = video_decoder_->NeedReConstruct(frame.type(), frame.frame_width(), frame.frame_height());
            if (need_init) {
                auto result = video_decoder_->Init(frame.type(), frame.frame_width(), frame.frame_height());
                if (result != 0) {
                    LOGI("Video decoder init failed!");
                    return;
                }
                LOGI("Create decoder success {}x{}, type: {}", frame.frame_width(), frame.frame_height(), (int)frame.type());
            }

            auto ret = video_decoder_->Decode(frame.data(), [=](const auto& raw_image) {
                if (!raw_image) {
                    return;
                }

                if (video_frame_cbk_) {
                    video_frame_cbk_(raw_image);
                }

                if (!first_frame_) {
                    first_frame_ = true;
                    SendFirstFrameMessage(raw_image);
                }
            });
            if (ret != 0) {
                LOGE("decode error: {}", ret);
            }
        });

        ws_client_->SetOnAudioFrameMsgCallback([=, this](const AudioFrame& frame) {

        });

        ws_client_->Start();
    }

    void ThunderSdk::Exit() {

    }

    void ThunderSdk::SendFirstFrameMessage(const std::shared_ptr<RawImage>& image) {
        MsgFirstFrameDecoded msg;
        msg.raw_image_ = image;
        msg_notifier_->SendAppMessage(msg);
    }

}