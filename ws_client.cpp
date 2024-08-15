//
// Created by RGAA on 2023-12-27.
//

#include "ws_client.h"

#include <utility>
#include "tc_common_new/log.h"
#include "tc_common_new/data.h"
#include "tc_common_new/thread.h"
#include "tc_common_new/file.h"

#include <asio2/websocket/ws_client.hpp>
#include <asio2/asio2.hpp>

namespace tc
{

    const int kMaxClientQueuedMessage = 4096;

    std::shared_ptr<WSClient> WSClient::Make(const std::string& ip, int port, const std::string& path) {
        return std::make_shared<WSClient>(ip, port, path);
    }

    WSClient::WSClient(const std::string& ip, int port, const std::string& path) {
        this->ip_ = ip;
        this->port_ = port;
        this->path_ = path;
        this->send_thread_ = Thread::Make("ws_send", 128);
        this->send_thread_->Poll();
    }

    WSClient::~WSClient() = default;

    void WSClient::Start() {
        client_ = std::make_shared<asio2::ws_client>();
        client_->set_auto_reconnect(true);
        client_->set_timeout(std::chrono::milliseconds(2000));

        timer_ = std::make_shared<asio2::timer>();
        timer_->start_timer("ws-heartbeat", std::chrono::seconds(1), [=, this]() {
            this->HeartBeat();
        });

        client_->bind_init([&]() {
            client_->ws_stream().binary(true);
            client_->ws_stream().set_option(
                    websocket::stream_base::decorator([](websocket::request_type &req) {
                        req.set(http::field::authorization, "websocket-client-authorization");}
                    )
            );

        }).bind_connect([&]() {
            if (asio2::get_last_error()) {
                LOGE("connect failure : {} {}", asio2::last_error_val(), asio2::last_error_msg().c_str());
            } else {
                LOGI("connect success : {} {} ", client_->local_address().c_str(), client_->local_port());
                if (conn_cbk_) {
                    conn_cbk_();
                }
            }
        }).bind_upgrade([&]() {
            if (asio2::get_last_error()) {
                LOGE("upgrade failure : {}, {}", asio2::last_error_val(), asio2::last_error_msg());
            }
        }).bind_recv([&](std::string_view data) {
            this->ParseMessage(data);
        });

        // the /ws is the websocket upgraged target
        if (!client_->async_start(this->ip_, this->port_, this->path_)) {
            LOGE("connect websocket server failure : {} {}", asio2::last_error_val(), asio2::last_error_msg().c_str());
        }
    }

    void WSClient::Exit() {
        timer_->stop_all_timers();
        send_thread_->Exit();
        if (client_) {
            LOGI("Queued message count: {}", queued_msg_count_.load());
            client_->stop();
        }
        LOGI("WS has exited...");
    }

    void WSClient::ParseMessage(std::string_view msg) {
        auto net_msg = std::make_shared<tc::Message>();
        bool ok = net_msg->ParseFromArray(msg.data(), msg.size());
        if (!ok) {
            LOGE("Sdk ParseMessage failed.");
            return;
        }

        if (net_msg->type() == tc::kVideoFrame) {
            const auto& video_frame = net_msg->video_frame();
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
        }
    }

    void WSClient::PostBinaryMessage(const std::string& msg) {
        if (!send_thread_) {
            return;
        }
        send_thread_->Post([=, this]() {
            if (!client_ || !client_->is_started()) {
               return;
            }
            if (queued_msg_count_ > kMaxClientQueuedMessage) {
                LOGW("queued so many message, discard this message in WSClient");
                return;
            }
            queued_msg_count_++;
            try {
                client_->async_send(msg, [=, this]() {
                    this->queued_msg_count_--;
                });
            } catch (std::exception &e) {
                LOGE("PostBinaryMessage(string) failed: {}", e.what());
            }
        });
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

    void WSClient::SetOnAudioSpectrumCallback(OnAudioSpectrumCallback&& cbk) {
        audio_spectrum_cbk_ = std::move(cbk);
    }

    void WSClient::SetOnHeartBeatCallback(tc::OnHeartBeatInfoCallback&& cbk) {
        hb_cbk_ = std::move(cbk);
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