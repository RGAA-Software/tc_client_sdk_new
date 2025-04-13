//
// Created by RGAA on 1/03/2025.
//

#include "relay_connection.h"
#include "tc_relay_client/relay_client_sdk.h"
#include "tc_relay_client/relay_net_client.h"
#include "relay_message.pb.h"
#include "tc_common_new/message_notifier.h"
#include "tc_client_sdk_new/sdk_messages.h"

using namespace relay;

namespace tc
{

    RelayConnection::RelayConnection(const std::shared_ptr<MessageNotifier>& notifier,
                                     const std::string& host,
                                     int port,
                                     const std::string& device_id,
                                     const std::string& remote_device_id,
                                     bool auto_relay) : Connection(notifier) {
        this->host_ = host;
        this->port_ = port;
        this->device_id_ = device_id;
        this->remote_device_id_ = remote_device_id;
        this->auto_relay_ = auto_relay;
        relay_sdk_ = std::make_shared<RelayClientSdk>(RelayClientSdkParam {
            .host_ = host,
            .port_ = port,
            .device_id_ = device_id,
            .remote_device_id_ = remote_device_id
        });

        relay_net_client_ = relay_sdk_->GetNetClient();

        relay_sdk_->SetOnRelayProtoMessageCallback([=, this](const std::shared_ptr<RelayMessage>& rl_msg) {
            if (rl_msg->type() == RelayMessageType::kRelayTargetMessage) {
                auto sub = rl_msg->relay();
                auto relay_msg_index = sub.relay_msg_index();
                if (msg_cbk_) {
                    auto payload = std::string(sub.payload());
                    msg_cbk_(std::move(payload));
                }
            }
        });

        relay_sdk_->SetOnRelayServerConnectedCallback([=, this]() {
            LOGI("Relay server connected.");
            if (conn_cbk_) {
                conn_cbk_();
            }
        });

        relay_sdk_->SetOnRelayServerDisConnectedCallback([=, this]() {
            LOGI("Relay server disconnected.");
            if (dis_conn_cbk_) {
                dis_conn_cbk_();
            }
        });

        relay_sdk_->SetOnRelayRoomPreparedCallback([=, this]() {
            LOGI("Auto relay: {}", auto_relay_);
            if (auto_relay_) {
                this->RequestResumeStream();
            }

            // notify
            msg_notifier_->SendAppMessage(SdkMsgRoomPrepared{});
        });

        relay_sdk_->SetOnRelayRoomDestroyedCallback([=, this]() {
            // notify
            msg_notifier_->SendAppMessage(SdkMsgRoomDestroyed{});
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

    int64_t RelayConnection::GetQueuingMsgCount() {
        if (relay_sdk_) {
            return relay_sdk_->GetQueuingMsgCount();
        }
        return Connection::GetQueuingMsgCount();
    }

    void RelayConnection::RequestPauseStream() {
        if (relay_sdk_) {
            relay_sdk_->RequestPauseStream();
        }
    }

    void RelayConnection::RequestResumeStream() {
        if (relay_sdk_) {
            relay_sdk_->RequestResumeStream();
        }
    }

}
