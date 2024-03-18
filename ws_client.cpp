//
// Created by RGAA on 2023-12-27.
//

#include "ws_client.h"

#include <utility>
#include "tc_common/log.h"
#include "tc_common/data.h"
#include "tc_common/thread.h"
#include "tc_common/file.h"

namespace tc
{

    std::shared_ptr<WSClient> WSClient::Make(const std::string& url) {
        return std::make_shared<WSClient>(url);
    }

    WSClient::WSClient(std::string url) : url_(std::move(url)) {

    }

    WSClient::~WSClient() {

    }

    void WSClient::Start() {
        auto conn_task = [=, this]() {
            while (!stop_connecting_) {
                client_ = std::make_shared<client>();
                LOGI("URL: {}", url_.c_str());

                try {
                    // Set logging to be pretty verbose (everything except message payloads)
                    client_->set_access_channels(websocketpp::log::alevel::all);
                    client_->clear_access_channels(websocketpp::log::alevel::frame_payload);

                    client_->init_asio();
                    client_->set_open_handler(std::bind(&WSClient::OnOpen, this, client_.get(), ::_1));
                    client_->set_close_handler(std::bind(&WSClient::OnClose, this, client_.get(), ::_1));
                    client_->set_fail_handler(std::bind(&WSClient::OnFailed, this, client_.get(), ::_1));
                    client_->set_message_handler(std::bind(&WSClient::OnMessage, this, client_.get(), ::_1, ::_2));

                    websocketpp::lib::error_code ec;
                    client::connection_ptr con = client_->get_connection(url_, ec);
                    if (ec) {
                        LOGW("could not create connection because: %s", ec.message().c_str());
                    }

                    // Note that connect here only requests a connection. No network messages are
                    // exchanged until the event loop starts running in the next line.
                    client_->connect(con);

                    // Start the ASIO io_service run loop
                    // this will cause a single connection to be made to the server. c.run()
                    // will exit when this connection is closed.
                    client_->run();

                    if (!stop_connecting_) {
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                    }
                } catch (websocketpp::exception const & e) {
                    LOGE("Websocket connect failed: {}, will retry.", e.what());
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }
        };

        ws_thread_ = std::make_shared<Thread>(conn_task, "ws_client", false);
    }

    void WSClient::Exit() {
        stop_connecting_ = true;
        if (client_) {
            client_->stop();
        }
        if (ws_thread_ && ws_thread_->IsJoinable()) {
            ws_thread_->Join();
        }
    }

    void WSClient::OnOpen(client* c, websocketpp::connection_hdl hdl) {
        target_server_ = hdl;
        LOGI("OnOpen");
    }

    void WSClient::OnClose(client* c, websocketpp::connection_hdl hdl) {
        target_server_.reset();
        LOGI("OnClose");
    }

    void WSClient::OnFailed(client* c, websocketpp::connection_hdl hdl) {
        target_server_.reset();
        LOGI("OnFailed");
    }

    void WSClient::OnMessage(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
        //LOGI("OnMessage: {}, size: {}", (int)msg->get_opcode(), msg->get_payload().size());
        this->ParseMessage(msg->get_payload());
    }

    void WSClient::ParseMessage(const std::string& msg) {
        auto net_msg = std::make_shared<tc::Message>();
        bool ok = net_msg->ParseFromString(msg);
        if (!ok) {
            LOGE("Sdk ParseMessage failed.");
            return;
        }

        if (net_msg->type() == tc::kVideoFrame) {
            const auto& video_frame = net_msg->video_frame();
            if (video_frame_cbk_) {
                video_frame_cbk_(video_frame);
            }
        }
        else if (net_msg->type() == tc::kAudioFrame) {
            const auto& audio_frame = net_msg->audio_frame();
            if (audio_frame_cbk_) {
                audio_frame_cbk_(audio_frame);
            }
        } else if (net_msg->type() == tc::kCursorInfoSync) {
            const auto& cursor_info = net_msg->cursor_info_sync();
            if(cursor_info_sync_cbk_) {
                cursor_info_sync_cbk_(cursor_info);
            }
        }
    }

    void WSClient::PostBinaryMessage(const std::string& msg) {
        if (client_ && target_server_.lock()) {
            client_->send(target_server_, msg, binary);
        }
    }

    void WSClient::PostBinaryMessage(const std::shared_ptr<Data>& msg) {
        if (client_ && target_server_.lock()) {
            client_->send(target_server_, msg->CStr(), msg->Size(), binary);
        }
    }

    void WSClient::PostTextMessage(const std::string& msg) {
        if (msg.empty()) {
            return;
        }
        if (client_ && target_server_.lock()) {
            client_->send(target_server_, msg, text);
        }
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
}