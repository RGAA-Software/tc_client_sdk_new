//
// Created by RGAA on 1/03/2025.
//

#include "relay_connection.h"
#include "tc_relay_client/relay_client_sdk.h"
#include "tc_relay_client/relay_net_client.h"
#include "relay_message.pb.h"

namespace tc
{

    RelayConnection::RelayConnection(const std::string& host, int port, const std::string& device_id, const std::string& remote_device_id) {
        this->host_ = host;
        this->port_ = port;
        this->device_id_ = device_id;
        this->remote_device_id_ = remote_device_id;
        relay_sdk_ = std::make_shared<RelayClientSdk>(RelayClientSdkParam {
            .host_ = host,
            .port_ = port,
            .device_id_ = device_id,
            .remote_device_id_ = remote_device_id
        });

        relay_net_client_ = relay_sdk_->GetNetClient();
        relay_sdk_->SetOnRelayProtoMessageCallback([](const std::shared_ptr<RelayMessage>& rl_msg) {
            LOGI("relay message in :{}", (int)rl_msg->type());
        });

        relay_net_client_->SetOnRelayServerConnectedCallback([=, this]() {
            LOGI("Relay server connected.");
        });

        relay_net_client_->SetOnRelayServerDisConnectedCallback([=, this]() {
            LOGI("Relay server disconnected.");
        });
    }

    void RelayConnection::Start() {
        relay_sdk_->Start();
    }

    void RelayConnection::Stop() {
        if (relay_sdk_) {
            relay_sdk_->Stop();
        }
    }

    void RelayConnection::PostBinaryMessage(const std::string& msg) {
        if (relay_sdk_) {
            relay_sdk_->RelayProtoMessage(msg);
        }
    }

}
