//
// Created by RGAA on 2023/12/26.
//

#include "thunder_sdk.h"

#include "tc_common_new/log.h"
#include "tc_common_new/file.h"
#include "tc_common_new/message_notifier.h"
#include "tc_common_new/thread.h"
#include "tc_common_new/time_util.h"
#include "tc_client_sdk_new/gl/raw_image.h"
#include "net_client.h"
#include "video_decoder_factory.h"
#include "tc_message.pb.h"
#include "sdk_messages.h"
#include "tc_opus_codec_new/opus_codec.h"
#include "cast_receiver.h"
#include "app_timer.h"
#include "statistics.h"

namespace tc
{

    std::string ThunderSdkParams::MakeReqPath() const {
        auto base_url = ip_ + ":" + std::to_string(port_) + media_path_;
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

        net_client_ = std::make_shared<NetClient>(msg_notifier_,
                                      sdk_params_.ip_,
                                      sdk_params_.port_,
                                      sdk_params_.media_path_,
                                      sdk_params_.ft_path_,
                                      sdk_params_.conn_type_,
                                      sdk_params_.nt_type_,
                                      sdk_params_.device_id_,
                                      sdk_params_.remote_device_id_,
                                      sdk_params_.ft_device_id_,
                                      sdk_params_.ft_remote_device_id_,
                                      sdk_params_.stream_id_);
        return true;
    }

    void ThunderSdk::Start() {
        statistics_ = Statistics::Instance();

        // threads
        video_thread_ = Thread::Make("video", 16);
        video_thread_->Poll();
        audio_thread_ = Thread::Make("audio", 16);
        audio_thread_->Poll();
        audio_spectrum_thread_ = Thread::Make("bg", 32);
        audio_spectrum_thread_->Poll();

        net_client_->SetOnConnectCallback([=, this]() {
            this->SendHelloMessage();
            msg_notifier_->SendAppMessage(SdkMsgNetworkConnected{});
        });

        net_client_->SetOnDisconnectedCallback([=, this]() {
            msg_notifier_->SendAppMessage(SdkMsgNetworkDisConnected{});
            has_config_msg_ = false;
            has_video_frame_msg_ = false;
        });

        net_client_->SetOnVideoFrameMsgCallback([=, this](const VideoFrame& frame) {
            if (exit_) { return; }
            this->PostVideoTask([=, this]() {
                const auto& monitor_name = frame.mon_name();
                std::shared_ptr<VideoDecoder> video_decoder = nullptr;
                if (video_decoders_.contains(monitor_name)) {
                    video_decoder = video_decoders_[monitor_name];
                    bool rebuild = video_decoder->NeedReConstruct(frame.type(), frame.frame_width(), frame.frame_height());
                    if (rebuild) {
                        video_decoder->Release();
                        video_decoders_.erase(monitor_name);
                        video_decoder = nullptr;
                        LOGI("Rebuild video decoder, type: {}, {}x{}", (int)frame.type(), frame.frame_width(), frame.frame_height());
                    }
                }
                if (!video_decoder) {
                    video_decoder = VideoDecoderFactory::Make((drt_ == DecoderRenderType::kMediaCodecSurface || drt_ == DecoderRenderType::kMediaCodecNv21) ? SupportedCodec::kMediaCodec : SupportedCodec::kFFmpeg);
                    bool ready = video_decoder->Ready();
                    if (!ready) {
                        auto result = video_decoder->Init(frame.type(), frame.frame_width(), frame.frame_height(), frame.data(), render_surface_);
                        if (result != 0) {
                            LOGI("Video decoder init failed!");
                            return;
                        }
                        LOGI("Create decoder success {}x{}, type: {}", frame.frame_width(), frame.frame_height(), (int)frame.type());
                    }
                    video_decoders_[monitor_name] = video_decoder;
                }

                auto current_time = TimeUtil::GetCurrentTimestamp();
                if (last_received_video_ == 0) {
                    last_received_video_ = current_time;
                }
                auto diff = current_time - last_received_video_;
                last_received_video_ = current_time;
                //LOGI("video msg received diff: {}", diff);
                statistics_->AppendVideoRecvGap(diff);
                statistics_->fps_video_recv_->Tick();
                statistics_->AppendMediaDataSize(frame.data().size());
                statistics_->render_width_ = frame.frame_width();
                statistics_->render_height_ = frame.frame_height();

                SdkCaptureMonitorInfo cap_mon_info {
                    .mon_name_ = frame.mon_name(),
                    .mon_index_ = frame.mon_index(),
                    .mon_left_ = frame.mon_left(),
                    .mon_top_ = frame.mon_top(),
                    .mon_right_ = frame.mon_right(),
                    .mon_bottom_ = frame.mon_bottom(),
                    .frame_width_ = frame.frame_width(),
                    .frame_height_ = frame.frame_height(),
                };

                static auto last_frame_index = frame.frame_index();
                auto frame_diff = frame.frame_index() - last_frame_index;
                if (frame_diff > 1) {
                    LOGI("Video frame came, index: {}, diff: {}", frame.frame_index(), frame_diff);
                }
                last_frame_index = frame.frame_index();

                auto ret = video_decoder->Decode(frame.data(), [=, this](const auto& raw_image) {
                    if (exit_) {
                        return;
                    }
                    if (!raw_image) {
                        return;
                    }
                    //LOGI("decode image size {}x{}", raw_image->img_width, raw_image->img_height);
                    if (video_frame_cbk_) {
                        video_frame_cbk_(raw_image, cap_mon_info);
                    }

                    if (!has_video_frame_msg_) {
                        has_video_frame_msg_ = true;
                        SendFirstFrameMessage(raw_image, cap_mon_info);
                    }
                });
                if (ret != 0) {
                    RequestIFrame();
                    LOGE("decode error: {}, will request Key Frame", ret);
                }
            });
        });

        net_client_->SetOnAudioFrameMsgCallback([=, this](const AudioFrame& frame) {
            if (exit_) { return; }
            this->PostAudioTask([=, this]() {
                auto beg = TimeUtil::GetCurrentTimestamp();
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
                auto end = TimeUtil::GetCurrentTimestamp();
                //LOGI("decode audio : {}", end-beg);
            });
        });

        net_client_->SetOnAudioSpectrumCallback([=, this](const tc::ServerAudioSpectrum& spectrum) {
            if (exit_) { return; }
            this->PostAudioSpectrumTask([=, this]() {
                if (audio_spectrum_cbk_) {
                    audio_spectrum_cbk_(spectrum);
                }
            });
        });

        net_client_->Start();

        // receiver
        // cast_receiver_ = CastReceiver::Make();
        // cast_receiver_->Start();

        app_timer_ = std::make_shared<AppTimer>(msg_notifier_);
        app_timer_->StartTimers();

        RegisterEventListeners();

    }

    void ThunderSdk::SendFirstFrameMessage(const std::shared_ptr<RawImage>& image, const SdkCaptureMonitorInfo& info) {
        SdkMsgFirstVideoFrameDecoded msg;
        msg.raw_image_ = image;
        msg.mon_info_ = info;
        msg_notifier_->SendAppMessage(msg);
    }

    void ThunderSdk::PostMediaMessage(const std::string& msg) {
        if(net_client_) {
            net_client_->PostMediaMessage(msg);
        }
    }

    void ThunderSdk::PostFileTransferMessage(const std::string& msg) {
        if(net_client_) {
            net_client_->PostFileTransferMessage(msg);
        }
    }

    void ThunderSdk::RegisterEventListeners() {
        msg_listener_ = msg_notifier_->CreateListener();
        msg_listener_->Listen<SdkMsgTimer100>([=, this](const auto& msg) {
            auto m = statistics_->AsProtoMessage(sdk_params_.device_id_, sdk_params_.stream_id_);
            this->PostMediaMessage(m);
        });
        msg_listener_->Listen<SdkMsgTimer1000>([=, this](const auto& msg) {
            statistics_->TickFps();
        });
        msg_listener_->Listen<SdkMsgTimer2000>([=, this](const auto& msg) {
            //this->statistics_->Dump();
        });
    }

    void ThunderSdk::SendHelloMessage() {
        if (!net_client_) {
            return;
        }
        tc::Message msg;
        msg.set_type(tc::MessageType::kHello);
        msg.set_device_id(sdk_params_.device_id_);
        msg.set_stream_id(sdk_params_.stream_id_);
        auto hello = msg.mutable_hello();
        hello->set_enable_audio(sdk_params_.enable_audio_);
        hello->set_enable_video(sdk_params_.enable_video_);
        hello->set_client_type(sdk_params_.client_type_);
        hello->set_enable_controller(sdk_params_.enable_controller_);
        net_client_->PostMediaMessage(msg.SerializeAsString());
    }

    void ThunderSdk::RequestIFrame() {
        if (!net_client_) {
            return;
        }
        tc::Message msg;
        msg.set_type(MessageType::kInsertKeyFrame);
        msg.set_device_id(sdk_params_.device_id_);
        msg.set_stream_id(sdk_params_.stream_id_);
        auto ack = msg.mutable_ack();
        ack->set_type(MessageType::kInsertKeyFrame);
        net_client_->PostMediaMessage(msg.SerializeAsString());
    }

    void ThunderSdk::PostVideoTask(std::function<void()>&& task) {
        video_thread_->Post(SimpleThreadTask::Make(std::move(task)));
    }

    void ThunderSdk::PostAudioTask(std::function<void()>&& task) {
        audio_thread_->Post(SimpleThreadTask::Make(std::move(task)));
    }

    void ThunderSdk::PostAudioSpectrumTask(std::function<void()>&& task) {
        audio_spectrum_thread_->Post(SimpleThreadTask::Make(std::move(task)));
    }

    void ThunderSdk::SetOnAudioSpectrumCallback(OnAudioSpectrumCallback&& cbk) {
        audio_spectrum_cbk_ = std::move(cbk);
    }

    void ThunderSdk::SetOnCursorInfoCallback(tc::OnCursorInfoSyncMsgCallback&& cbk) {
        if (net_client_) {
            net_client_->SetOnCursorInfoSyncMsgCallback(std::move(cbk));
        }
    }

    void ThunderSdk::SetOnHeartBeatCallback(tc::OnHeartBeatInfoCallback&& cbk) {
        if (net_client_) {
            net_client_->SetOnHeartBeatCallback(std::move(cbk));
        }
    }

    void ThunderSdk::SetOnClipboardCallback(OnClipboardInfoCallback&& cbk) {
        if (net_client_) {
            net_client_->SetOnClipboardCallback(std::move(cbk));
        }
    }

    void ThunderSdk::SetOnServerConfigurationCallback(OnConfigCallback&& cbk) {
        if (net_client_) {
            net_client_->SetOnServerConfigurationCallback(std::move(cbk));

            if (!has_config_msg_) {
                has_config_msg_ = true;
                msg_notifier_->SendAppMessage(SdkMsgFirstConfigInfoCallback());
            }
        }
    }

    void ThunderSdk::SetOnMonitorSwitchedCallback(OnMonitorSwitchedCallback&& cbk) {
        if (net_client_) {
            net_client_->SetOnMonitorSwitchedCallback(std::move(cbk));
        }
    }

    void ThunderSdk::SetOnRawMessageCallback(OnRawMessageCallback&& cbk) {
        if (net_client_) {
            net_client_->SetOnRawMessageCallback(std::move(cbk));
        }
    }

    int ThunderSdk::GetProgressSteps() const {
        if (sdk_params_.conn_type_ == ClientConnectType::kDirect) {
            if (sdk_params_.nt_type_ == ClientNetworkType::kWebsocket) {
                return 3;
            } else if (sdk_params_.nt_type_ == ClientNetworkType::kUdpKcp) {
                return 3;
            } else if (sdk_params_.nt_type_ == ClientNetworkType::kWebRtc) {
                return 3;
            }
        }
        else {
            if (sdk_params_.nt_type_ == ClientNetworkType::kWebRtc) {
                return 3;
            }
        }
        return 0;
    }

    ThunderSdkParams ThunderSdk::GetSdkParams() {
        return sdk_params_;
    }

    int64_t ThunderSdk::GetQueuingMediaMsgCount() {
        if (net_client_) {
            return net_client_->GetQueuingMediaMsgCount();
        }
        return 0;
    }

    int64_t ThunderSdk::GetQueuingFtMsgCount() {
        if (net_client_) {
            return net_client_->GetQueuingFtMsgCount();
        }
        return 0;
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
        if (net_client_) {
            net_client_->Exit();
        }

        LOGI("will exit video decoder.");
        for (const auto& [mon_name, video_decoder] : video_decoders_) {
            if (video_decoder) {
                video_decoder->Release();
            }
        }

        LOGI("Will exit video thread.");
        if (video_thread_) {
            video_thread_->Exit();
        }
        LOGI("Will exit audio thread.");
        if (audio_thread_) {
            audio_thread_->Exit();
        }
        LOGI("Will exit audio_spectrum_thread thread");
        if (audio_spectrum_thread_) {
            audio_spectrum_thread_->Exit();
        }

        LOGI("ThunderSdk exited");
    }
}