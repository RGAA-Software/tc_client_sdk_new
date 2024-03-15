//
// Created by hy on 2023/12/26.
//

#ifndef TC_CLIENT_PC_THUNDERSDK_H
#define TC_CLIENT_PC_THUNDERSDK_H

#include <iostream>
#include <string>

#include "tc_message.pb.h"
#include "decoder_render_type.h"

namespace tc
{
    class Data;
    class Thread;
    class WSClient;
    class VideoDecoder;
    class RawImage;
    class MessageNotifier;
    class OpusAudioDecoder;
    class WebRtcClient;
    class CastReceiver;

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
    using OnAudioFrameDecodedCallback = std::function<void(const std::shared_ptr<Data>&, int samples, int channels, int bits)>;

    class ThunderSdk {
    public:

        static std::shared_ptr<ThunderSdk> Make(const std::shared_ptr<MessageNotifier>& notifier);

        explicit ThunderSdk(const std::shared_ptr<MessageNotifier>& notifier);
        ~ThunderSdk();

        bool Init(const ThunderSdkParams& params, void* surface, const DecoderRenderType& drt);
        void Start();
        void Exit();

        void RegisterOnVideoFrameDecodedCallback(OnVideoFrameDecodedCallback&& cbk) { this->video_frame_cbk_ = std::move(cbk); }
        void RegisterOnAudioFrameDecodedCallback(OnAudioFrameDecodedCallback&& cbk) { this->audio_frame_cbk_ = std::move(cbk); }

        // to do 需要在这里专门添加一个线程 用来发送消息吗？ 还是直接发
        void PostBinaryMessage(const std::string& msg);

    private:

        void SendFirstFrameMessage(const std::shared_ptr<RawImage>& image);

    private:
        std::shared_ptr<MessageNotifier> msg_notifier_ = nullptr;
        ThunderSdkParams sdk_params_;
        std::shared_ptr<WSClient> ws_client_ = nullptr;
        std::shared_ptr<VideoDecoder> video_decoder_ = nullptr;

        // for android
        void* render_surface_ = nullptr;

        // callbacks
        OnVideoFrameDecodedCallback video_frame_cbk_;
        OnAudioFrameDecodedCallback audio_frame_cbk_;

        bool first_frame_ = false;
        DecoderRenderType drt_;
        bool exit_ = false;

        std::shared_ptr<OpusAudioDecoder> audio_decoder_ = nullptr;
        bool debug_audio_decoder_ = false;

        std::shared_ptr<WebRtcClient> webrtc_client_ = nullptr;

        std::shared_ptr<CastReceiver> cast_receiver_ = nullptr;

    };

}

#endif //TC_CLIENT_PC_THUNDERSDK_H
