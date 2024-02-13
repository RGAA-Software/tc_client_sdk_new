//
// Created by hy on 2023/12/26.
//

#include "thunder_sdk.h"

#include "tc_common/log.h"
#include "tc_common/file.h"
#include "tc_common/message_notifier.h"
#include "tc_client_sdk/gl/raw_image.h"

#include "ws_client.h"
#include "video_decoder_factory.h"
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

    bool ThunderSdk::Init(const ThunderSdkParams& params, void* surface, const DecoderRenderType& drt) {
        sdk_params_ = params;
        drt_ = drt;
        render_surface_ = surface;
        return true;
    }

    void ThunderSdk::Start() {
        // video decoder
        video_decoder_ = VideoDecoderFactory::Make((drt_ == DecoderRenderType::kMediaCodecSurface|| drt_ == DecoderRenderType::kMediaCodecNv21) ? SupportedCodec::kMediaCodec : SupportedCodec::kFFmpeg);

        // websocket client
        ws_client_ = WSClient::Make(sdk_params_.MakeReqPath());
        ws_client_->SetOnVideoFrameMsgCallback([=](const VideoFrame& frame) {
            if (exit_) {
                return;
            }
            bool need_init = video_decoder_->NeedReConstruct(frame.type(), frame.frame_width(), frame.frame_height());
            if (need_init) {
                auto result = video_decoder_->Init(frame.type(), frame.frame_width(), frame.frame_height(), frame.data(), render_surface_);
                if (result != 0) {
                    LOGI("Video decoder init failed!");
                    return;
                }
                LOGI("Create decoder success {}x{}, type: {}", frame.frame_width(), frame.frame_height(), (int)frame.type());
            }

            auto ret = video_decoder_->Decode(frame.data(), [=](const auto& raw_image) {
                if (exit_) {
                    return;
                }
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

        ws_client_->SetOnAudioFrameMsgCallback([=](const AudioFrame& frame) {

        });

        ws_client_->Start();
    }

    void ThunderSdk::Exit() {
        exit_ = true;

        if (ws_client_) {
            ws_client_->Exit();
        }

        if (video_decoder_) {
            video_decoder_->Release();
        }
    }

    void ThunderSdk::SendFirstFrameMessage(const std::shared_ptr<RawImage>& image) {
        MsgFirstFrameDecoded msg;
        msg.raw_image_ = image;
        msg_notifier_->SendAppMessage(msg);
    }

    void ThunderSdk::PostBinaryMessage(const std::string& msg) {
        if(ws_client_) {
            ws_client_->PostBinaryMessage(msg);
        }
    }

}