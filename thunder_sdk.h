//
// Created by hy on 2023/12/26.
//

#ifndef TC_CLIENT_PC_THUNDERSDK_H
#define TC_CLIENT_PC_THUNDERSDK_H

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <iostream>
#include <string>

#include "tc_message.pb.h"

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using websocketpp::frame::opcode::value::binary;
using websocketpp::frame::opcode::value::text;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

namespace tc
{
    class Data;
    class Thread;

    using OnVideoFrameMsgCallback = std::function<void(const VideoFrame& frame)>;
    using OnAudioFrameMsgCallback = std::function<void(const AudioFrame& frame)>;

    class ThunderSdkParams {
    public:
        [[nodiscard]] std::string MakeReqPath() const;

    public:
        bool ssl_ = false;
        std::string ip_;
        int port_;
        std::string req_path_;
    };

    class ThunderSdk {
    public:
        ThunderSdk();
        ~ThunderSdk();

        bool Init(const ThunderSdkParams& params);
        void Start();

        void PostBinaryMessage(const std::string& msg);
        void PostBinaryMessage(const std::shared_ptr<Data>& msg);
        void PostTextMessage(const std::string& msg);

        void SetOnVideoFrameMsgCallback(OnVideoFrameMsgCallback&& cbk);
        void SetOnAudioFrameMsgCallback(OnAudioFrameMsgCallback&& cbk);

    private:
        void OnOpen(client* c, websocketpp::connection_hdl hdl);
        void OnClose(client* c, websocketpp::connection_hdl hdl);
        void OnFailed(client* c, websocketpp::connection_hdl hdl);
        void OnMessage(client* c, websocketpp::connection_hdl hdl, message_ptr msg);

        void ParseMessage(const std::string& msg);

    private:

        std::shared_ptr<client> client_ = nullptr;
        websocketpp::connection_hdl target_server_;

        OnVideoFrameMsgCallback video_frame_cbk_;
        OnAudioFrameMsgCallback audio_frame_cbk_;

        bool stop_connecting_ = false;

    };

}

#endif //TC_CLIENT_PC_THUNDERSDK_H
