//
// Created by RGAA on 2023-12-27.
//

#include "ws_client.h"

#include <utility>
#include "tc_common_new/log.h"
#include "tc_common_new/data.h"
#include "tc_common_new/thread.h"
#include "tc_common_new/file.h"
#include "tc_common_new/message_notifier.h"
#include "sdk_messages.h"
#include "udp_connection.h"
#include "ws_connection.h"

#include <asio2/websocket/ws_client.hpp>
#include <asio2/asio2.hpp>

namespace tc
{

    const int kMaxClientQueuedMessage = 4096;

    std::shared_ptr<WSClient> WSClient::Make(const std::shared_ptr<MessageNotifier>& notifier, const std::string& ip, int port, const std::string& path) {
        return std::make_shared<WSClient>(notifier, ip, port, path);
    }

    WSClient::WSClient(const std::shared_ptr<MessageNotifier>& notifier, const std::string& ip, int port, const std::string& path) {
        this->msg_notifier_ = notifier;
        this->ip_ = ip;
        this->port_ = port;
        this->path_ = path;

        msg_listener_ = msg_notifier_->CreateListener();
        msg_listener_->Listen<MsgTimer2000>([=, this](const auto& msg) {
            this->HeartBeat();
        });
    }

    WSClient::~WSClient() = default;

    void WSClient::Start() {
        conn_ = std::make_shared<WsConnection>(ip_, port_, path_);
        //conn_ = std::make_shared<UdpConnection>("127.0.0.1", 20400);
        conn_->RegisterOnConnectedCallback([=, this]() {
            if (conn_cbk_) {
                conn_cbk_();
            }
        });

        conn_->RegisterOnDisConnectedCallback([=, this]() {
            if (dis_conn_cbk_) {
                dis_conn_cbk_();
            }
        });

        conn_->RegisterOnMessageCallback([=, this](std::string&& data) {
            this->ParseMessage(std::move(data));
        });

        conn_->Start();
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
//        if (!client_->async_start(this->ip_, this->port_, this->path_)) {
//            LOGE("connect websocket server failure : {} {}", asio2::last_error_val(), asio2::last_error_msg().c_str());
//        }

    }

    void WSClient::Exit() {
        if (conn_) {
            LOGI("Queued message count: {}", queued_msg_count_.load());
            conn_->Stop();
        }
        LOGI("WS has exited...");
    }

    void WSClient::ParseMessage(std::string&& msg) {
        auto net_msg = std::make_shared<tc::Message>();
        bool ok = net_msg->ParseFromArray(msg.data(), msg.size());
        if (!ok) {
            LOGE("Sdk ParseMessage failed.");
            return;
        }

        if (net_msg->type() == tc::kVideoFrame) {
            const auto& video_frame = net_msg->video_frame();
            LOGI("video frame index: {}, {}x{}, key: {}", video_frame.frame_index(),
                 video_frame.frame_width(), video_frame.frame_height(), video_frame.key());
            if (video_frame_cbk_) {
                video_frame_cbk_(video_frame);
            }
//            static std::ofstream file("1234.h264", std::ios::binary);
//            file.write(video_frame.data().c_str(), video_frame.data().size());

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
            msg_notifier_->SendAppMessage(MsgChangeMonitorResolutionResult {
                .monitor_name_ = sub.monitor_name(),
                .result = sub.result(),
            });
        }
    }

    void WSClient::PostBinaryMessage(const std::string& msg) {
        if (conn_) {
            conn_->PostBinaryMessage(msg);
        }
//        if (!client_ || !client_->is_started()) {
//            return;
//        }
//        if (queued_msg_count_ > kMaxClientQueuedMessage) {
//            LOGW("queued so many message, discard this message in WSClient");
//            return;
//        }
//        queued_msg_count_++;
//        try {
//            client_->async_send(msg, [=, this]() {
//                this->queued_msg_count_--;
//            });
//        } catch (std::exception &e) {
//            LOGE("PostBinaryMessage(string) failed: {}", e.what());
//        }
    }

    void WSClient::PostBinaryMessage(const std::shared_ptr<Data>& msg) {
        this->PostBinaryMessage(msg->AsString());
    }

    void WSClient::SetOnVideoFrameMsgCallback(OnVideoFrameMsgCallback&& cbk) {
        video_frame_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnAudioFrameMsgCallback(OnAudioFrameMsgCallback&& cbk) {
        audio_frame_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnCursorInfoSyncMsgCallback(OnCursorInfoSyncMsgCallback&& cbk) {
        cursor_info_sync_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnConnectCallback(OnConnectedCallback&& cbk) {
        conn_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnDisconnectedCallback(OnDisconnectedCallback&& cbk) {
        dis_conn_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnAudioSpectrumCallback(OnAudioSpectrumCallback&& cbk) {
        audio_spectrum_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnHeartBeatCallback(tc::OnHeartBeatInfoCallback&& cbk) {
        hb_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnClipboardCallback(OnClipboardInfoCallback&& cbk) {
        clipboard_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnServerConfigurationCallback(tc::OnConfigCallback&& cbk) {
        config_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnMonitorSwitchedCallback(OnMonitorSwitchedCallback&& cbk) {
        monitor_switched_cbk_ = std::move(cbk);
    }

    void WSClient::HeartBeat() {
        auto msg = std::make_shared<Message>();
        msg->set_type(tc::kHeartBeat);
        auto hb = msg->mutable_heartbeat();
        hb->set_index(hb_idx_++);
        auto proto_msg = msg->SerializeAsString();
        this->PostBinaryMessage(proto_msg);
    }
}