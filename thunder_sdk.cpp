//
// Created by hy on 2023/12/26.
//

#include "thunder_sdk.h"

#include "tc_common_new/log.h"
#include "tc_common_new/file.h"
#include "tc_common_new/message_notifier.h"
#include "tc_client_sdk_new/gl/raw_image.h"
#include "ws_client.h"
#include "video_decoder_factory.h"
#include "tc_message.pb.h"
#include "sdk_messages.h"
#include "tc_opus_codec_new/opus_codec.h"
#include "cast_receiver.h"
#ifdef WIN32
#include "webrtc_client.h"
#endif

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
        // webrtc
#ifdef WIN32
        webrtc_client_ = WebRtcClient::Make();
        webrtc_client_->Start("127.0.0.1", 9002);
#endif

        // websocket client
        ws_client_ = WSClient::Make(sdk_params_.MakeReqPath());
        ws_client_->SetOnVideoFrameMsgCallback([=, this](const VideoFrame& frame) {
            if (exit_) {
                return;
            }
            // video decoder
            if (!video_decoder_) {
                video_decoder_ = VideoDecoderFactory::Make((drt_ == DecoderRenderType::kMediaCodecSurface || drt_ == DecoderRenderType::kMediaCodecNv21) ? SupportedCodec::kMediaCodec : SupportedCodec::kFFmpeg);
            }
            bool ready = video_decoder_->Ready();
            if (!ready) {
                auto result = video_decoder_->Init(frame.type(), frame.frame_width(), frame.frame_height(), frame.data(), render_surface_);
                if (result != 0) {
                    LOGI("Video decoder init failed!");
                    return;
                }
                LOGI("Create decoder success {}x{}, type: {}", frame.frame_width(), frame.frame_height(), (int)frame.type());
            }

            auto ret = video_decoder_->Decode(frame.data(), [=, this](const auto& raw_image) {
                if (exit_) {
                    return;
                }
                if (!raw_image) {
                    return;
                }
                //LOGI("decode image size {}x{}", raw_image->img_width, raw_image->img_height);
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
            if (exit_) {
                return;
            }
            if (!audio_decoder_) {
                audio_decoder_ = std::make_shared<OpusAudioDecoder>(frame.samples(), frame.channels());
            }
            std::vector<unsigned char> buffer(frame.data().begin(), frame.data().end());
            auto pcm_data = audio_decoder_->Decode(buffer, frame.frame_size(), false);
            if (audio_frame_cbk_) {
                auto data = Data::Make((char*)pcm_data.data(), pcm_data.size()*2);
                audio_frame_cbk_(data, frame.samples(), frame.channels(), frame.bits());
            }
            //LOGI("opus data size: {}, frame size: {}, samples: {}, channel: {}, PCM data size in char : {}", frame.data().size(), frame.frame_size(), frame.samples(), frame.channels(), pcm_data.size()*2);
            if (debug_audio_decoder_) {
                static FilePtr pcm_audio = File::OpenForWriteB("1.test.pcm");
                pcm_audio->Append((char *) pcm_data.data(), pcm_data.size()*2);
            }
        });

        ws_client_->SetOnCursorInfoSyncMsgCallback([=](const CursorInfoSync& cursor_info) {
            if(cursor_info_sync_callback_) {
                cursor_info_sync_callback_(cursor_info);
            }
        });

        ws_client_->Start();

        // receiver
        cast_receiver_ = CastReceiver::Make();
        cast_receiver_->Start();
    }

    void ThunderSdk::Exit() {
        exit_ = true;
        if (cast_receiver_) {
            cast_receiver_->Exit();
        }

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