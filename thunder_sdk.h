//
// Created by RGAA on 2023/12/26.
//

#ifndef TC_CLIENT_PC_THUNDERSDK_H
#define TC_CLIENT_PC_THUNDERSDK_H

#include <iostream>
#include <string>

#include "tc_message.pb.h"
#include "sdk_params.h"
#include "sdk_messages.h"
#include "sdk_net_client.h"
#include "sdk_decoder_render_type.h"

namespace tc
{
    class Data;
    class Thread;
    class VideoDecoder;
    class RawImage;
    class MessageNotifier;
    class MessageListener;
    class OpusAudioDecoder;
    class WebRtcClient;
    class CastReceiver;
    class SdkTimer;
    class SdkStatistics;

    // callbacks
    using OnVideoFrameDecodedCallback = std::function<void(const std::shared_ptr<RawImage>&, const SdkCaptureMonitorInfo&)>;
    using OnAudioFrameDecodedCallback = std::function<void(const std::shared_ptr<Data>&, int samples, int channels, int bits)>;

    class ThunderSdk {
    public:

        static std::shared_ptr<ThunderSdk> Make(const std::shared_ptr<MessageNotifier>& notifier);

        explicit ThunderSdk(const std::shared_ptr<MessageNotifier>& notifier);
        ~ThunderSdk();

        bool Init(const std::shared_ptr<ThunderSdkParams>& params, void* surface, const DecoderRenderType& drt);
        void Start();
        void Exit();

        void SetOnVideoFrameDecodedCallback(OnVideoFrameDecodedCallback&& cbk) { this->video_frame_cbk_ = std::move(cbk); }
        void SetOnAudioFrameDecodedCallback(OnAudioFrameDecodedCallback&& cbk) { this->audio_frame_cbk_ = std::move(cbk); }
        void SetOnAudioSpectrumCallback(OnAudioSpectrumCallback&& cbk);
        void SetOnCursorInfoCallback(OnCursorInfoSyncMsgCallback&& cbk);
        void SetOnHeartBeatCallback(OnHeartBeatInfoCallback&& cbk);
        void SetOnClipboardCallback(OnClipboardInfoCallback&& cbk);
        void SetOnServerConfigurationCallback(OnConfigCallback&& cbk);
        void SetOnMonitorSwitchedCallback(OnMonitorSwitchedCallback&& cbk);
        void SetOnRawMessageCallback(OnRawMessageCallback&& cbk);

        void PostMediaMessage(const std::string& msg);
        void PostFileTransferMessage(const std::string& msg);
        void PostVideoTask(std::function<void()>&& task);
        void PostAudioTask(std::function<void()>&& task);
        // !! @Deprecated
        void PostAudioSpectrumTask(std::function<void()>&& task);

        int GetProgressSteps() const;
        std::shared_ptr<ThunderSdkParams> GetSdkParams();
        std::shared_ptr<MessageNotifier> GetMessageNotifier();
        int64_t GetQueuingMediaMsgCount();
        int64_t GetQueuingFtMsgCount();

    private:
        void SendFirstFrameMessage(const std::shared_ptr<RawImage>& image, const SdkCaptureMonitorInfo& info);
        void RegisterEventListeners();
        void SendHelloMessage();
        void RequestIFrame();
        void ReportStatistics();

    private:
        std::shared_ptr<MessageNotifier> msg_notifier_ = nullptr;
        std::shared_ptr<ThunderSdkParams> sdk_params_;
        std::shared_ptr<NetClient> net_client_ = nullptr;
        std::map<std::string, std::shared_ptr<VideoDecoder>> video_decoders_;

        // for android
        void* render_surface_ = nullptr;

        // callbacks
        OnVideoFrameDecodedCallback video_frame_cbk_;
        OnAudioFrameDecodedCallback audio_frame_cbk_;
        OnAudioSpectrumCallback audio_spectrum_cbk_;

        DecoderRenderType drt_;
        bool exit_ = false;

        std::shared_ptr<OpusAudioDecoder> audio_decoder_ = nullptr;
        bool debug_audio_decoder_ = false;

        std::shared_ptr<WebRtcClient> webrtc_client_ = nullptr;
        std::shared_ptr<CastReceiver> cast_receiver_ = nullptr;
        std::shared_ptr<SdkTimer> sdk_timer_ = nullptr;
        std::shared_ptr<MessageListener> msg_listener_ = nullptr;
        SdkStatistics* statistics_ = nullptr;
        std::shared_ptr<Thread> video_thread_ = nullptr;
        std::shared_ptr<Thread> audio_thread_ = nullptr;
        // !! @Deprecated
        std::shared_ptr<Thread> audio_spectrum_thread_ = nullptr;

        std::map<std::string, uint64_t> last_received_video_timestamps_ ;

        std::atomic_bool has_config_msg_ = false;
        std::atomic_bool has_video_frame_msg_ = false;

    };

}

#endif //TC_CLIENT_PC_THUNDERSDK_H
