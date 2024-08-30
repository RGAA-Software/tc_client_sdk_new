//
// Created by RGAA on 2023-12-27.
//

#ifndef TC_CLIENT_PC_WS_CLIENT_H
#define TC_CLIENT_PC_WS_CLIENT_H

#include "tc_message.pb.h"
#include <atomic>

namespace asio2 {
    class ws_client;
    class timer;
}

namespace tc
{

    using OnVideoFrameMsgCallback = std::function<void(const VideoFrame& frame)>;
    using OnAudioFrameMsgCallback = std::function<void(const AudioFrame& frame)>;
    using OnCursorInfoSyncMsgCallback = std::function<void(const CursorInfoSync& cursor_info)>;
    using OnAudioSpectrumCallback = std::function<void(const tc::ServerAudioSpectrum&)>;
    using OnConnectedCallback = std::function<void()>;
    using OnDisconnectedCallback = std::function<void()>;
    using OnHeartBeatInfoCallback = std::function<void(const tc::OnHeartBeat&)>;
    using OnClipboardInfoCallback = std::function<void(const tc::ClipboardInfo&)>;
    using OnConfigCallback = std::function<void(const tc::ServerConfiguration&)>;
    using OnMonitorSwitchedCallback = std::function<void(const tc::MonitorSwitched&)>;

    class Data;
    class Thread;
    class MessageNotifier;

    class WSClient {
    public:

        static std::shared_ptr<WSClient> Make(const std::shared_ptr<MessageNotifier>& notifier, const std::string& ip, int port, const std::string& path);

        explicit WSClient(const std::shared_ptr<MessageNotifier>& notifier, const std::string& ip, int port, const std::string& path);
        ~WSClient();

        void Start();
        void Exit();

        void PostBinaryMessage(const std::string& msg);
        void PostBinaryMessage(const std::shared_ptr<Data>& msg);

        void SetOnVideoFrameMsgCallback(OnVideoFrameMsgCallback&& cbk);
        void SetOnAudioFrameMsgCallback(OnAudioFrameMsgCallback&& cbk);
        void SetOnCursorInfoSyncMsgCallback(OnCursorInfoSyncMsgCallback&& cbk);
        void SetOnAudioSpectrumCallback(OnAudioSpectrumCallback&& cbk);
        void SetOnConnectCallback(OnConnectedCallback&& cbk);
        void SetOnDisconnectedCallback(OnDisconnectedCallback&& cbk);
        void SetOnHeartBeatCallback(OnHeartBeatInfoCallback&& cbk);
        void SetOnClipboardCallback(OnClipboardInfoCallback&& cbk);
        void SetOnServerConfigurationCallback(OnConfigCallback&& cbk);
        void SetOnMonitorSwitchedCallback(OnMonitorSwitchedCallback&& cbk);

    private:
        void ParseMessage(std::string_view msg);
        void HeartBeat();

    private:
        std::shared_ptr<asio2::ws_client> client_ = nullptr;
        OnVideoFrameMsgCallback video_frame_cbk_;
        OnAudioFrameMsgCallback audio_frame_cbk_;
        OnCursorInfoSyncMsgCallback cursor_info_sync_cbk_;
        OnAudioSpectrumCallback audio_spectrum_cbk_;
        OnConnectedCallback conn_cbk_;
        OnDisconnectedCallback dis_conn_cbk_;
        OnHeartBeatInfoCallback hb_cbk_;
        OnClipboardInfoCallback clipboard_cbk_;
        OnConfigCallback config_cbk_;
        OnMonitorSwitchedCallback monitor_switched_cbk_;

        std::string ip_{};
        int port_{};
        std::string path_{};

        std::atomic_int queued_msg_count_ = 0;
        uint64_t hb_idx_ = 0;

        std::shared_ptr<MessageNotifier> msg_notifier_ = nullptr;

    };

}

#endif //TC_CLIENT_PC_WS_CLIENT_H
