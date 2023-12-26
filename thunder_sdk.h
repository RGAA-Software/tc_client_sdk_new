//
// Created by hy on 2023/12/26.
//

#ifndef TC_CLIENT_PC_THUNDERSDK_H
#define TC_CLIENT_PC_THUNDERSDK_H

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <iostream>
#include <string>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

namespace tc
{
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

    private:
        void OnOpen(client* c, websocketpp::connection_hdl hdl);
        void OnClose(client* c, websocketpp::connection_hdl hdl);
        void OnFailed(client* c, websocketpp::connection_hdl hdl);
        void OnMessage(client* c, websocketpp::connection_hdl hdl, message_ptr msg);

        void ParseMessage(const std::string& msg);

    private:

        std::shared_ptr<client> client_ = nullptr;

    };

}

#endif //TC_CLIENT_PC_THUNDERSDK_H
