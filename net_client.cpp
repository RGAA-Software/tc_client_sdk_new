//
// Created by RGAA on 2023-12-27.
//

#include "net_client.h"

#include <utility>
#include "tc_common_new/log.h"
#include "tc_common_new/data.h"
#include "tc_common_new/thread.h"
#include "tc_common_new/file.h"
#include "tc_common_new/message_notifier.h"
#include "sdk_messages.h"
#include "connection/udp_connection.h"
#include "connection/ws_connection.h"
#include "connection/relay_connection.h"

#include <asio2/websocket/ws_client.hpp>
#include <asio2/asio2.hpp>

namespace tc
{

    NetClient::NetClient(const std::shared_ptr<MessageNotifier>& notifier,
                         const std::string& ip,
                         int port,
                         const std::string& media_path,
                         const std::string& ft_path,
                         const ClientConnectType& conn_type,
                         const ClientNetworkType& nt_type,
                         const std::string& device_id,
                         const std::string& remote_device_id,
                         const std::string& ft_device_id,
                         const std::string& ft_remote_device_id,
                         const std::string& stream_id) {
        this->msg_notifier_ = notifier;
        this->ip_ = ip;
        this->port_ = port;
        this->media_path_ = media_path;
        this->ft_path_ = ft_path;
        this->conn_type_ = conn_type;
        this->network_type_ = nt_type;
        this->device_id_ = device_id;
        this->remote_device_id_ = remote_device_id;
        this->ft_device_id_ = ft_device_id;
        this->ft_remote_device_id_ = ft_remote_device_id;
        this->stream_id_ = stream_id;

        msg_listener_ = msg_notifier_->CreateListener();
        msg_listener_->Listen<SdkMsgTimer2000>([=, this](const auto& msg) {
            this->HeartBeat();
        });
    }

    NetClient::~NetClient() = default;

    void NetClient::Start() {
        if (network_type_ == ClientNetworkType::kWebsocket) {
            LOGI("Will connect by Websocket:");
            LOGI("media: {}", media_path_);
            LOGI("file transfer: {}", ft_path_);
            media_conn_ = std::make_shared<WsConnection>(ip_, port_, media_path_);
            ft_conn_ = std::make_shared<WsConnection>(ip_, port_, ft_path_);
        }
        else if (network_type_ == ClientNetworkType::kUdpKcp) {
            LOGI("Will connect by UDP");
            media_conn_ = std::make_shared<UdpConnection>(ip_, port_);
        }
        else if (network_type_ == ClientNetworkType::kRelay) {
            media_conn_ = std::make_shared<RelayConnection>(ip_, port_, device_id_, remote_device_id_);
            ft_conn_ = std::make_shared<RelayConnection>(ip_, port_, ft_device_id_, ft_remote_device_id_);
        }
        else {
            LOGE("Start failed! Don't know the connection type: {}", (int)network_type_);
            return;
        }

        media_conn_->RegisterOnConnectedCallback([=, this]() {
            if (conn_cbk_) {
                conn_cbk_();
            }
        });

        media_conn_->RegisterOnDisConnectedCallback([=, this]() {
            if (dis_conn_cbk_) {
                dis_conn_cbk_();
            }
        });

        media_conn_->RegisterOnMessageCallback([=, this](std::string&& data) {
            this->ParseMessage(std::move(data));
        });

        media_conn_->Start();
        if (ft_conn_) {
            ft_conn_->RegisterOnMessageCallback([=, this](std::string&& data) {
                this->ParseMessage(std::move(data));
            });
            ft_conn_->Start();
        }
//
//        client_ = std::make_shared<asio2::ws_client>();
//        client_->set_auto_reconnect(true);
//        client_->set_timeout(std::chrono::milliseconds(2000));
//        client_->start_timer("ws-heartbeat", std::chrono::seconds(1), [=, this]() {
//            this->HeartBeat();
//        });
//
//        client_->bind_init([=, this]() {
//            client_->ws_stream().binary(true);
//            client_->set_no_delay(true);
//            client_->ws_stream().set_option(
//                    websocket::stream_base::decorator([](websocket::request_type &req) {
//                        req.set(http::field::authorization, "websocket-client-authorization");}
//                    )
//            );
//
//        }).bind_connect([=, this]() {
//            if (asio2::get_last_error()) {
//                LOGE("connect failure : {} {}", asio2::last_error_val(), asio2::last_error_msg().c_str());
//            } else {
//                LOGI("connect success : {} {} ", client_->local_address().c_str(), client_->local_port());
//                client_->post_queued_event([=, this]() {
//                    if (conn_cbk_) {
//                        conn_cbk_();
//                    }
//                });
//            }
//        }).bind_disconnect([this]() {
//            if (dis_conn_cbk_) {
//                dis_conn_cbk_();
//            }
//        }).bind_upgrade([]() {
//            if (asio2::get_last_error()) {
//                LOGE("upgrade failure : {}, {}", asio2::last_error_val(), asio2::last_error_msg());
//            }
//        }).bind_recv([=, this](std::string_view data) {
//            this->ParseMessage(data);
//        });
//
//        // the /ws is the websocket upgraged target
//        if (!client_->async_start(this->ip_, this->port_, this->media_path_)) {
//            LOGE("connect websocket server failure : {} {}", asio2::last_error_val(), asio2::last_error_msg().c_str());
//        }

    }

    void NetClient::Exit() {
        if (media_conn_) {
            LOGI("Queued message count: {}", queuing_message_count_.load());
            media_conn_->Stop();
        }
        if (ft_conn_) {
            ft_conn_->Stop();
        }
        LOGI("WS has exited...");
    }

    void NetClient::ParseMessage(std::string&& msg) {
        auto net_msg = std::make_shared<tc::Message>();
        bool ok = net_msg->ParseFromArray(msg.data(), msg.size());
        if (!ok) {
            LOGE("Sdk ParseMessage failed.");
            return;
        }

        if (raw_msg_cbk_) {
            raw_msg_cbk_(net_msg);
        }

        if (net_msg->type() == tc::kVideoFrame) {
            const auto& video_frame = net_msg->video_frame();
            if (video_frame.key()) {
                //LOGI("video frame index: {}, {}x{}, key: {}", video_frame.frame_index(),
                //     video_frame.frame_width(), video_frame.frame_height(), video_frame.key());
            }
            if (video_frame_cbk_) {
                video_frame_cbk_(video_frame);
            }

        } else if (net_msg->type() == tc::kAudioFrame) {
            const auto& audio_frame = net_msg->audio_frame();
            if (audio_frame_cbk_) {
                audio_frame_cbk_(audio_frame);
            }
        } else if (net_msg->type() == tc::kCursorInfoSync) {
            const auto& cursor_info = net_msg->cursor_info_sync();
            if(cursor_info_sync_cbk_) {
                cursor_info_sync_cbk_(cursor_info);
            }
        } else if (net_msg->type() == tc::kServerAudioSpectrum) {
            const auto& spectrum = net_msg->server_audio_spectrum();
            if (audio_spectrum_cbk_) {
                audio_spectrum_cbk_(spectrum);
            }
        } else if (net_msg->type() == tc::kOnHeartBeat) {
            const auto& hb = net_msg->on_heartbeat();
            if (hb_cbk_) {
                hb_cbk_(hb);
            }
        } else if (net_msg->type() == tc::kClipboardInfo) {
            const auto& clipboard_info = net_msg->clipboard_info();
            if (clipboard_cbk_) {
                clipboard_cbk_(clipboard_info);
            }
        } else if (net_msg->type() == tc::kServerConfiguration) {
            const auto& config = net_msg->config();
            if (config_cbk_) {
                config_cbk_(config);
            }
        } else if (net_msg->type() == tc::kMonitorSwitched) {
            const auto& monitor_switched = net_msg->monitor_switched();
            if (monitor_switched_cbk_) {
                monitor_switched_cbk_(monitor_switched);
            }
        } else if (net_msg->type() == tc::kChangeMonitorResolutionResult) {
            auto sub = net_msg->change_monitor_resolution_result();
            msg_notifier_->SendAppMessage(SdkMsgChangeMonitorResolutionResult {
                .monitor_name_ = sub.monitor_name(),
                .result = sub.result(),
            });
        } else if (net_msg->type() == tc::kClipboardReqBuffer) {
            auto sub = net_msg->cp_req_buffer();
            msg_notifier_->SendAppMessage(SdkMsgClipboardReqBuffer {
                .req_buffer_ = sub,
            });
        }
    }

    void NetClient::PostMediaMessage(const std::string& msg) {
        if (media_conn_) {
            media_conn_->PostBinaryMessage(msg);
        }
    }

    void NetClient::PostFileTransferMessage(const std::string& msg) {
        if (ft_conn_) {
            ft_conn_->PostBinaryMessage(msg);
        }
    }

    void NetClient::SetOnVideoFrameMsgCallback(OnVideoFrameMsgCallback&& cbk) {
        video_frame_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnAudioFrameMsgCallback(OnAudioFrameMsgCallback&& cbk) {
        audio_frame_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnCursorInfoSyncMsgCallback(OnCursorInfoSyncMsgCallback&& cbk) {
        cursor_info_sync_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnConnectCallback(OnConnectedCallback&& cbk) {
        conn_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnDisconnectedCallback(OnDisconnectedCallback&& cbk) {
        dis_conn_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnAudioSpectrumCallback(OnAudioSpectrumCallback&& cbk) {
        audio_spectrum_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnHeartBeatCallback(tc::OnHeartBeatInfoCallback&& cbk) {
        hb_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnClipboardCallback(OnClipboardInfoCallback&& cbk) {
        clipboard_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnServerConfigurationCallback(tc::OnConfigCallback&& cbk) {
        config_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnMonitorSwitchedCallback(OnMonitorSwitchedCallback&& cbk) {
        monitor_switched_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnRawMessageCallback(tc::OnRawMessageCallback&& cbk) {
        raw_msg_cbk_ = std::move(cbk);
    }

    void NetClient::HeartBeat() {
        auto msg = std::make_shared<Message>();
        msg->set_type(tc::kHeartBeat);
        msg->set_device_id(device_id_);
        msg->set_stream_id(stream_id_);
        auto hb = msg->mutable_heartbeat();
        hb->set_index(hb_idx_++);
        auto proto_msg = msg->SerializeAsString();
        this->PostMediaMessage(proto_msg);
    }

    int64_t NetClient::GetQueuingMediaMsgCount() {
        if (media_conn_) {
            return media_conn_->GetQueuingMsgCount();
        }
        return 0;
    }

    int64_t NetClient::GetQueuingFtMsgCount() {
        if (ft_conn_) {
            return ft_conn_->GetQueuingMsgCount();
        }
        return 0;
    }
}