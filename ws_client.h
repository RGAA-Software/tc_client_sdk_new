//
// Created by RGAA on 2023-12-27.
//

#ifndef TC_CLIENT_PC_WS_CLIENT_H
#define TC_CLIENT_PC_WS_CLIENT_H

#include "tc_message.pb.h"
#include <atomic>
#include <asio2/websocket/ws_client.hpp>

namespace tc
{

    using OnVideoFrameMsgCallback = std::function<void(const VideoFrame& frame)>;
    using OnAudioFrameMsgCallback = std::function<void(const AudioFrame& frame)>;
    using OnCursorInfoSyncMsgCallback = std::function<void(const CursorInfoSync& cursor_info)>;

    class Data;
    class Thread;

    class WSClient {
    public:

        static std::shared_ptr<WSClient> Make(const std::string& ip, int port, const std::string& path);

        explicit WSClient(const std::string& ip, int port, const std::string& path);
        ~WSClient();

        void Start();
        void Exit();

        void PostBinaryMessage(const std::string& msg);
        void PostBinaryMessage(const std::shared_ptr<Data>& msg);

        void SetOnVideoFrameMsgCallback(OnVideoFrameMsgCallback&& cbk);
        void SetOnAudioFrameMsgCallback(OnAudioFrameMsgCallback&& cbk);
        void SetOnCursorInfoSyncMsgCallback(OnCursorInfoSyncMsgCallback&& cbk);
    private:

        void ParseMessage(std::string_view msg);

    private:
        std::shared_ptr<asio2::ws_client> client_ = nullptr;
        OnVideoFrameMsgCallback video_frame_cbk_;
        OnAudioFrameMsgCallback audio_frame_cbk_;
        OnCursorInfoSyncMsgCallback cursor_info_sync_cbk_;

        std::string ip_{};
        int port_{};
        std::string path_{};

        std::atomic_int queued_msg_count_ = 0;

    };

}

#endif //TC_CLIENT_PC_WS_CLIENT_H
