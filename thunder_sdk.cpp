//
// Created by hy on 2023/12/26.
//

#include "thunder_sdk.h"

#include "tc_common_new/log.h"
#include "tc_common_new/file.h"
#include "tc_common_new/message_notifier.h"
#include "tc_common_new/thread.h"
#include "tc_common_new/time_ext.h"
#include "tc_client_sdk_new/gl/raw_image.h"
#include "ws_client.h"
#include "video_decoder_factory.h"
#include "tc_message.pb.h"
#include "sdk_messages.h"
#include "tc_opus_codec_new/opus_codec.h"
#include "cast_receiver.h"
#include "app_timer.h"
#include "statistics.h"

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
        statistics_ = Statistics::Instance();

        // threads
        video_thread_ = Thread::Make("video", 8);
        video_thread_->Poll();
        audio_thread_ = Thread::Make("audio", 8);
        audio_thread_->Poll();
        bg_thread_ = Thread::Make("bg", 32);
        bg_thread_->Poll();

        // websocket client
        ws_client_ = WSClient::Make(sdk_params_.ip_, sdk_params_.port_, sdk_params_.req_path_);
        ws_client_->SetOnVideoFrameMsgCallback([=, this](const VideoFrame& frame) {
            this->PostVideoTask([=, this]() {
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

                auto current_time = TimeExt::GetCurrentTimestamp();
                if (last_received_video_ == 0) {
                    last_received_video_ = current_time;
                }
                auto diff = current_time - last_received_video_;
                last_received_video_ = current_time;
                LOGI("video msg received diff: {}", diff);
                statistics_->AppendVideoRecvGap(diff);
                statistics_->fps_video_recv_->Tick();

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
        });

        ws_client_->SetOnAudioFrameMsgCallback([=, this](const AudioFrame& frame) {
            this->PostAudioTask([=, this]() {
                if (exit_) {
                    return;
                }
                auto beg = TimeExt::GetCurrentTimestamp();
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
                auto end = TimeExt::GetCurrentTimestamp();
                LOGI("decode audio : {}", end-beg);
            });
        });

        ws_client_->SetOnCursorInfoSyncMsgCallback([=, this](const CursorInfoSync& cursor_info) {
            if(cursor_info_sync_callback_) {
                cursor_info_sync_callback_(cursor_info);
            }
        });

        ws_client_->Start();

        // receiver
        // cast_receiver_ = CastReceiver::Make();
        // cast_receiver_->Start();

        app_timer_ = std::make_shared<AppTimer>(msg_notifier_);
        app_timer_->StartTimers();

        RegisterEventListeners();

    }

    void ThunderSdk::Exit() {
        LOGI("ThunderSdk start exiting.");
        exit_ = true;
        if (cast_receiver_) {
            cast_receiver_->Exit();
        }

        LOGI("Will exit app timer.");
        if (app_timer_) {
            app_timer_->Exit();
        }

        LOGI("Will exit ws client.");
        if (ws_client_) {
            ws_client_->Exit();
        }

        LOGI("will exit video decoder.");
        if (video_decoder_) {
            video_decoder_->Release();
        }

        LOGI("Will exit video thread.");
        if (video_thread_) {
            video_thread_->Exit();
        }
        LOGI("Will exit audio thread.");
        if (audio_thread_) {
            audio_thread_->Exit();
        }
        LOGI("Will exit bg thread");
        if (bg_thread_) {
            bg_thread_->Exit();
        }

        LOGI("after ThunderSdk exiting");
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

    void ThunderSdk::RegisterEventListeners() {
        msg_listener_ = msg_notifier_->CreateListener();
        msg_listener_->Listen<MsgTimer100>([=, this](const auto& msg) {
            auto m = statistics_->AsProtoMessage();
            this->PostBinaryMessage(m);
        });
        msg_listener_->Listen<MsgTimer2000>([=, this](const auto& msg) {
            this->statistics_->Dump();
        });
    }

    void ThunderSdk::PostVideoTask(std::function<void()>&& task) {
        video_thread_->Post(SimpleThreadTask::Make(std::move(task)));
    }

    void ThunderSdk::PostAudioTask(std::function<void()>&& task) {
        audio_thread_->Post(SimpleThreadTask::Make(std::move(task)));
    }

    void ThunderSdk::PostBgTask(std::function<void()>&& task) {
        bg_thread_->Post(SimpleThreadTask::Make(std::move(task)));
    }

}