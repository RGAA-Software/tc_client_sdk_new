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
#include "tc_opus_codec_new/opus_codec.h"
#include "tc_message.pb.h"
#include "sdk_timer.h"
#include "sdk_messages.h"
#include "sdk_statistics.h"
#include "sdk_net_client.h"
#include "sdk_cast_receiver.h"
#include "sdk_video_decoder_factory.h"
#include "sdk_ffmpeg_soft_decoder.h"
#include "sdk_ffmpeg_d3d11va_decoder.h"
#include "tc_message_new/proto_converter.h"
#ifdef WIN32
#include "tc_common_new/hardware.h"
#endif

namespace tc
{

    std::shared_ptr<ThunderSdk> ThunderSdk::Make(const std::shared_ptr<MessageNotifier>& notifier) {
        return std::make_shared<ThunderSdk>(notifier);
    }

    ThunderSdk::ThunderSdk(const std::shared_ptr<MessageNotifier>& notifier) {
        this->msg_notifier_ = notifier;
    }

    ThunderSdk::~ThunderSdk() {

    }

    bool ThunderSdk::Init(const std::shared_ptr<ThunderSdkParams>& params, void* surface, const DecoderRenderType& drt) {
        sdk_params_ = params;
        drt_ = drt;
        render_surface_ = surface;

        auto fn_process_target_platform = [&]() {
            #if defined(_WIN32)
                sdk_params_->device_name_ = Hardware::GetDesktopName();
                sdk_params_->client_type_ = ClientType::kWindows;
                return ClientType::kWindows;
            #elif defined(__APPLE__)
                #include "TargetConditionals.h"
                #if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
                    // iOS或iOS模拟器
                    return ClientType::kiOS;
                #elif TARGET_OS_MAC
                    // macOS
                    return ClientType::kMacOS;
                #endif
            #elif defined(__ANDROID__)
                // Android
                return ClientType::kAndroid;
            #elif defined(__linux__)
                // Linux (not Android)
                return ClientType::kLinux;
            #else
                return ClientType::kUnknown;
            #endif
        };
        fn_process_target_platform();

        net_client_ = std::make_shared<NetClient>(sdk_params_,
                                      msg_notifier_,
                                      sdk_params_->ip_,
                                      sdk_params_->port_,
                                      sdk_params_->media_path_,
                                      sdk_params_->ft_path_,
                                      sdk_params_->nt_type_,
                                      sdk_params_->device_id_,
                                      sdk_params_->remote_device_id_,
                                      sdk_params_->ft_device_id_,
                                      sdk_params_->ft_remote_device_id_,
                                      sdk_params_->stream_id_);
        return true;
    }

    void ThunderSdk::Start() {
        statistics_ = SdkStatistics::Instance();

        // threads
        video_thread_ = Thread::Make("video", 16);
        video_thread_->Poll();
        audio_thread_ = Thread::Make("audio", 16);
        audio_thread_->Poll();
        misc_thread_ = Thread::Make("misc", 32);
        misc_thread_->Poll();

        net_client_->SetOnConnectCallback([=, this]() {
            this->SendHelloMessage();
            msg_notifier_->SendAppMessage(SdkMsgNetworkConnected{});
        });

        net_client_->SetOnDisconnectedCallback([=, this]() {
            msg_notifier_->SendAppMessage(SdkMsgNetworkDisConnected{});
            this->ClearFirstFrameState();
        });

        net_client_->SetOnVideoFrameMsgCallback([=, this](std::shared_ptr<tc::Message> msg) {
            if (exit_) { return; }
            this->PostVideoTask([=, this]() {
                auto frame = msg->video_frame();
                const auto& monitor_name = frame.mon_name();
                std::shared_ptr<VideoDecoder> video_decoder = nullptr;
                if (video_decoders_.contains(monitor_name)) {
                    video_decoder = video_decoders_[monitor_name];
                    bool rebuild = video_decoder->NeedReConstruct(frame.type(), frame.frame_width(), frame.frame_height(), frame.image_format());
                    if (rebuild) {
                        video_decoder->Release();
                        video_decoders_.erase(monitor_name);
                        video_decoder = nullptr;
                        LOGI("Rebuild video decoder, type: {}, {}x{}, image_format: {}", (int)frame.type(), frame.frame_width(), frame.frame_height(), (int)frame.image_format());
                    }
                }
                if (!video_decoder) {
#if ANDROID
                    // Some Android devices can't decode 2 or more streams at the same time, so, re-create it .
                    if (!video_decoders_.empty()) {
                        for (auto& [mon_name, decoder] : video_decoders_) {
                            decoder->Release();
                        }
                        video_decoders_.clear();
                    }
#endif
                    auto codec = (drt_ == DecoderRenderType::kMediaCodecSurface || drt_ == DecoderRenderType::kMediaCodecNv21) ? SupportedCodec::kMediaCodec : SupportedCodec::kFFmpeg;
                    video_decoder = VideoDecoderFactory::Make(shared_from_this(), codec);
#if TEST_D3D11VA
                    test_decoder_ = std::make_shared<FFmpegD3D11VADecoder>(shared_from_this());
                    test_decoder_->Init(frame.mon_name(), frame.type(), frame.frame_width(), frame.frame_height(), frame.data(), render_surface_, frame.image_format());
#endif

                    bool ready = video_decoder->Ready();
                    if (!ready) {
                        auto result = video_decoder->Init(frame.mon_name(), frame.type(), frame.frame_width(), frame.frame_height(), frame.data(), render_surface_, frame.image_format());
                        if (result != 0) {
                            LOGE("Video decoder init failed, mon name: {}, frame type: {}, frame width: {}, frame height: {}, format: {}",
                                 frame.mon_name(), (int)frame.type(), frame.frame_width(), frame.frame_height(), (int)frame.image_format());
                            return;
                        }
                        LOGI("Create decoder success {}x{}, type: {}", frame.frame_width(), frame.frame_height(), (int)frame.type());
                    }
                    video_decoders_[monitor_name] = video_decoder;
                }

                auto current_time = TimeUtil::GetCurrentTimestamp();
                if (!last_received_video_timestamps_.contains(monitor_name)) {
                    last_received_video_timestamps_[monitor_name] = current_time;
                }
                auto diff = current_time - last_received_video_timestamps_[monitor_name];
                last_received_video_timestamps_[monitor_name] = current_time;

                this->PostMiscTask([=, this]() {
                    statistics_->AppendVideoRecvGap(frame.mon_name(), diff);
                    statistics_->TickVideoRecvFps(frame.mon_name());
                    statistics_->UpdateFrameSize(frame.mon_name(), frame.frame_width(), frame.frame_height());
                });

                SdkCaptureMonitorInfo cap_mon_info{
                    .mon_name_ = frame.mon_name(),
                    .mon_index_ = frame.mon_index(),
                    .mon_left_ = frame.mon_left(),
                    .mon_top_ = frame.mon_top(),
                    .mon_right_ = frame.mon_right(),
                    .mon_bottom_ = frame.mon_bottom(),
                    .frame_width_ = frame.frame_width(),
                    .frame_height_ = frame.frame_height(),
                    .update_time_ = TimeUtil::GetCurrentTimestamp()
                };

                static auto last_frame_index = frame.frame_index();
                auto frame_diff = frame.frame_index() - last_frame_index;
                if (frame_diff > 1) {
                    LOGI("Video frame came, index: {}, diff: {}", frame.frame_index(), frame_diff);
                }
                last_frame_index = frame.frame_index();

#if TEST_D3D11VA
                // test // beg
                if (test_decoder_) {
                    test_decoder_->Decode(frame.data());
                }
                // test // end
#endif

                auto ret = video_decoder->Decode(frame.data());
                if (!ret.has_value() && ret.error() != 0) {
                    RequestIFrame();
                    LOGE("decode error: {}, will request Key Frame", ret.error());
                }
                if (exit_ || !ret.has_value()) {
                    return;
                }
                auto raw_image = ret.value();
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
        });

        net_client_->SetOnAudioFrameMsgCallback([=, this](std::shared_ptr<tc::Message> msg) {
            if (exit_) { return; }
            this->PostAudioTask([=, this]() {
                auto frame = msg->audio_frame();
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

        net_client_->SetOnAudioSpectrumCallback([=, this](std::shared_ptr<tc::Message> msg) {
            if (exit_) { return; }
            this->PostMiscTask([=, this]() {
                if (audio_spectrum_cbk_) {
                    audio_spectrum_cbk_(msg);
                }
            });
        });

        net_client_->Start();

        // receiver
        // cast_receiver_ = CastReceiver::Make();
        // cast_receiver_->Start();

        sdk_timer_ = std::make_shared<SdkTimer>(msg_notifier_);
        sdk_timer_->StartTimers();

        RegisterEventListeners();

    }

    void ThunderSdk::SendFirstFrameMessage(std::shared_ptr<RawImage> image, const SdkCaptureMonitorInfo& info) {
        SdkMsgFirstVideoFrameDecoded msg;
        msg.raw_image_ = image;
        msg.mon_info_ = info;
        msg_notifier_->SendAppMessage(msg);
    }

    void ThunderSdk::PostMediaMessage(std::shared_ptr<Data> msg) {
        if(net_client_ && msg) {
            net_client_->PostMediaMessage(msg);
        }
    }

    void ThunderSdk::PostFileTransferMessage(std::shared_ptr<Data> msg) {
        if(net_client_) {
            net_client_->PostFileTransferMessage(msg);
        }
    }

    void ThunderSdk::RegisterEventListeners() {
        msg_listener_ = msg_notifier_->CreateListener();

        // notify to net client
        msg_listener_->Listen<SdkMsgTimer16>([=, this](const auto& msg) {
            net_client_->On16msTimeout();
        });

        msg_listener_->Listen<SdkMsgTimer100>([=, this](const auto& msg) {

        });

        msg_listener_->Listen<SdkMsgTimer1000>([=, this](const auto& msg) {
            PostMiscTask([this]() {
                statistics_->CalculateDataSpeed();
                statistics_->CalculateVideoFrameFps();
                this->ReportStatistics();
            });
        });

        msg_listener_->Listen<SdkMsgTimer2000>([=, this](const auto& msg) {

        });

        // remote device offline
        msg_listener_->Listen<SdkMsgRelayRemoteDeviceOffline>([=, this](const SdkMsgRelayRemoteDeviceOffline& msg) {
            this->ClearFirstFrameState();
        });
    }

    void ThunderSdk::SendHelloMessage() {
        if (!net_client_) {
            return;
        }
        tc::Message msg;
        msg.set_type(tc::MessageType::kHello);
        msg.set_device_id(sdk_params_->device_id_);
        msg.set_stream_id(sdk_params_->stream_id_);
        auto hello = msg.mutable_hello();
        hello->set_enable_audio(sdk_params_->enable_audio_);
        hello->set_enable_video(sdk_params_->enable_video_);
        hello->set_client_type(sdk_params_->client_type_);
        hello->set_enable_controller(sdk_params_->enable_controller_);
        hello->set_device_name(sdk_params_->device_name_);
        if (auto buffer = tc::ProtoAsData(&msg); buffer) {
            net_client_->PostMediaMessage(buffer);
        }
    }

    void ThunderSdk::RequestIFrame() {
        if (!net_client_) {
            return;
        }
        tc::Message msg;
        msg.set_type(MessageType::kInsertKeyFrame);
        msg.set_device_id(sdk_params_->device_id_);
        msg.set_stream_id(sdk_params_->stream_id_);
        auto ack = msg.mutable_ack();
        ack->set_type(MessageType::kInsertKeyFrame);
        if (auto buffer = tc::ProtoAsData(&msg); buffer) {
            net_client_->PostMediaMessage(buffer);
        }
    }

    void ThunderSdk::PostVideoTask(std::function<void()>&& task) {
        video_thread_->Post(SimpleThreadTask::Make(std::move(task)));
    }

    void ThunderSdk::PostAudioTask(std::function<void()>&& task) {
        audio_thread_->Post(SimpleThreadTask::Make(std::move(task)));
    }

    void ThunderSdk::PostMiscTask(std::function<void()>&& task) {
        misc_thread_->Post(SimpleThreadTask::Make(std::move(task)));
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
            net_client_->SetOnServerConfigurationCallback([=, this](std::shared_ptr<tc::Message>&& msg) {
                cbk(std::move(msg));
                if (!has_config_msg_) {
                    has_config_msg_ = true;
                    msg_notifier_->SendAppMessage(SdkMsgFirstConfigInfoCallback());
                }
            });
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
        if (sdk_params_->nt_type_ == ClientNetworkType::kWebsocket) {
            return 3;
        }
        else if (sdk_params_->nt_type_ == ClientNetworkType::kUdpKcp) {
            return 3;
        }
        else if (sdk_params_->nt_type_ == ClientNetworkType::kWebRtc) {
            return 3;
        }
        else {
            return 3;
        }
    }

    std::shared_ptr<ThunderSdkParams> ThunderSdk::GetSdkParams() {
        return sdk_params_;
    }

    std::shared_ptr<MessageNotifier> ThunderSdk::GetMessageNotifier() {
        return msg_notifier_;
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

    void ThunderSdk::RetryConnection() {
        if (net_client_) {
            net_client_->RetryConnection();

            // notify reconnecting
            msg_notifier_->SendAppMessage(SdkMsgReconnect{});
        }
    }

    void ThunderSdk::ReportStatistics() {

    }

    void ThunderSdk::ClearFirstFrameState() {
        has_config_msg_ = false;
        has_video_frame_msg_ = false;
    }

    void ThunderSdk::Exit() {
        LOGI("ThunderSdk start exiting.");
        exit_ = true;
        if (cast_receiver_) {
            cast_receiver_->Exit();
        }

        LOGI("Will exit app timer.");
        if (sdk_timer_) {
            sdk_timer_->Exit();
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
        if (misc_thread_) {
            misc_thread_->Exit();
        }

        LOGI("ThunderSdk exited");
    }
}
