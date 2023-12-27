//
// Created by hy on 2023/12/26.
//

#include "thunder_sdk.h"

#include "tc_common/log.h"
#include "tc_common/file.h"

namespace tc
{

    std::string ThunderSdkParams::MakeReqPath() const {
        auto base_url = ip_ + ":" + std::to_string(port_) + req_path_;
        return ssl_ ? "wss://" +  base_url : "ws://" + base_url;
    }

    ///
    ThunderSdk::ThunderSdk() {

    }

    ThunderSdk::~ThunderSdk() {

    }

    bool ThunderSdk::Init(const ThunderSdkParams& params) {
        auto conn_task = [=, this]() {
            while (!stop_connecting_) {
                client_ = std::make_shared<client>();
                auto url = params.MakeReqPath();
                LOGI("URL: {}", url);

                try {
                    // Set logging to be pretty verbose (everything except message payloads)
                    client_->set_access_channels(websocketpp::log::alevel::all);
                    client_->clear_access_channels(websocketpp::log::alevel::frame_payload);

                    client_->init_asio();
                    client_->set_open_handler(std::bind(&ThunderSdk::OnOpen, this, client_.get(), ::_1));
                    client_->set_close_handler(std::bind(&ThunderSdk::OnClose, this, client_.get(), ::_1));
                    client_->set_fail_handler(std::bind(&ThunderSdk::OnFailed, this, client_.get(), ::_1));
                    client_->set_message_handler(std::bind(&ThunderSdk::OnMessage, this, client_.get(), ::_1, ::_2));

                    websocketpp::lib::error_code ec;
                    client::connection_ptr con = client_->get_connection(url, ec);
                    if (ec) {
                        std::cout << "could not create connection because: " << ec.message() << std::endl;
                    }

                    // Note that connect here only requests a connection. No network messages are
                    // exchanged until the event loop starts running in the next line.
                    client_->connect(con);

                    // Start the ASIO io_service run loop
                    // this will cause a single connection to be made to the server. c.run()
                    // will exit when this connection is closed.
                    client_->run();
                } catch (websocketpp::exception const & e) {
                    std::cout << e.what() << std::endl;
                }
            }
        };

        return true;
    }

    void ThunderSdk::Start() {

    }

    void ThunderSdk::OnOpen(client* c, websocketpp::connection_hdl hdl) {
        target_server_ = hdl;
        LOGI("OnOpen");
    }

    void ThunderSdk::OnClose(client* c, websocketpp::connection_hdl hdl) {
        target_server_.reset();
        LOGI("OnClose");
    }

    void ThunderSdk::OnFailed(client* c, websocketpp::connection_hdl hdl) {
        target_server_.reset();
        LOGI("OnFailed");
    }

    void ThunderSdk::OnMessage(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
        //LOGI("OnMessage: {}, size: {}", (int)msg->get_opcode(), msg->get_payload().size());
        this->ParseMessage(msg->get_payload());
    }

    //static FilePtr video_file = File::OpenForAppendB("1.recv.h264");
    void ThunderSdk::ParseMessage(const std::string& msg) {
        auto net_msg = std::make_shared<tc::Message>();
        bool ok = net_msg->ParseFromString(msg);
        if (!ok) {
            LOGE("Sdk ParseMessage failed.");
            return;
        }

        if (net_msg->type() == tc::kVideoFrame) {
            const auto& video_frame = net_msg->video_frame();
            LOGI("video: {} x {}, index: {}", video_frame.frame_width(), video_frame.frame_height(), video_frame.frame_index());
            if (video_frame_cbk_) {
                video_frame_cbk_(video_frame);
            }
            //auto data = net_msg->video_frame().data();
            //video_file->Append(data);
        }

    }

    void ThunderSdk::PostBinaryMessage(const std::string& msg) {
        if (client_ && target_server_.lock()) {
            client_->send(target_server_, msg, binary);
        }
    }

    void ThunderSdk::PostBinaryMessage(const std::shared_ptr<Data>& msg) {
        if (client_ && target_server_.lock()) {
            client_->send(target_server_, msg->CStr(), msg->Size(), binary);
        }
    }

    void ThunderSdk::PostTextMessage(const std::string& msg) {
        if (msg.empty()) {
            return;
        }
        if (client_ && target_server_.lock()) {
            client_->send(target_server_, msg, text);
        }
    }

    void ThunderSdk::SetOnVideoFrameMsgCallback(OnVideoFrameMsgCallback&& cbk) {
        video_frame_cbk_ = std::move(cbk);
    }

    void ThunderSdk::SetOnAudioFrameMsgCallback(OnAudioFrameMsgCallback&& cbk) {
        audio_frame_cbk_ = std::move(cbk);
    }
}