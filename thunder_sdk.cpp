//
// Created by hy on 2023/12/26.
//

#include "thunder_sdk.h"

#include "tc_common/log.h"
#include "tc_common/file.h"

#include "ws_client.h"
#include "ffmpeg_video_decoder.h"
#include "raw_image.h"
#include "tc_message.pb.h"

namespace tc
{

    std::string ThunderSdkParams::MakeReqPath() const {
        auto base_url = ip_ + ":" + std::to_string(port_) + req_path_;
        return ssl_ ? "wss://" +  base_url : "ws://" + base_url;
    }

    ///

    std::shared_ptr<ThunderSdk> ThunderSdk::Make() {
        return std::make_shared<ThunderSdk>();
    }

    ThunderSdk::ThunderSdk() {

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

            auto raw_image = video_decoder_->Decode(frame.data());
            if (!raw_image) {
                LOGE("Decode failed!");
                return;
            }

            if (video_frame_cbk_) {
                video_frame_cbk_(raw_image);
            }

        });

        ws_client_->SetOnAudioFrameMsgCallback([=, this](const AudioFrame& frame) {

        });

        ws_client_->Start();
    }

    void ThunderSdk::Exit() {

    }

}