//
// Created by RGAA on 10/08/2026.
//

#include "webrtc_local_connection.h"
#include "tc_common_new/log.h"
#include "tc_common_new/data.h"
#include "tc_common_new/thread.h"
#include "tc_common_new/time_util.h"
#include "tc_common_new/http_client.h"
#include "tc_common_new/md5.h"
#include "tc_common_new/message_notifier.h"
#include "tc_client_sdk_new/sdk_messages.h"
#include "tc_message.pb.h"
#include "tc_webrtc_client/rtc_client_interface.h"
#include <nlohmann/json.hpp>
#ifdef WIN32
#include <QApplication>
#endif

typedef void *(*FnGetInstance)();

namespace tc
{

    // render side error code, see gr_render/plugins/net_ws/http_handler.cpp(kHandlerErrRtcLocalOccupied)
    static constexpr int kRtcLocalRespOccupied = 704;

    // webrtc::PeerConnectionInterface::IceConnectionState, mirrored as int
    // to keep webrtc types out of the sdk
    static constexpr int kIceStateConnected = 2;    // kIceConnectionConnected
    static constexpr int kIceStateCompleted = 3;    // kIceConnectionCompleted
    static constexpr int kIceStateFailed = 4;       // kIceConnectionFailed
    static constexpr int kIceStateDisconnected = 5; // kIceConnectionDisconnected
    static constexpr int kIceStateClosed = 6;       // kIceConnectionClosed

    WebRtcLocalConnection::WebRtcLocalConnection(const std::shared_ptr<ThunderSdkParams>& params,
                                                 const std::shared_ptr<MessageNotifier>& notifier)
                                                 : Connection(params, notifier) {
        sdk_params_ = params;
        msg_notifier_ = notifier;
        thread_ = Thread::Make("rtc_local_conn", 1024 * 8);
        thread_->Poll();
    }

    WebRtcLocalConnection::~WebRtcLocalConnection() {

    }

    void WebRtcLocalConnection::Start() {
        RunInRtcThread([=, this]() {
            this->LoadRtcLibrary();
            this->InitRtcClient();
        });
    }

    void WebRtcLocalConnection::Stop() {
        if (stopped_.exchange(true)) {
            return;
        }
        if (rtc_client_) {
            rtc_client_->Exit();
        }
        if (thread_) {
            thread_->Exit();
        }
    }

    void WebRtcLocalConnection::LoadRtcLibrary() {
#ifdef WIN32
        LOGI("Begin to load rtc library!");
        auto lib_name = QApplication::applicationDirPath() + "/gr_client/tc_rtc_client.dll";
        rtc_lib_ = new QLibrary(lib_name);
        auto r = rtc_lib_->load();
        if (!r) {
            LOGE("LOAD rtc library FAILED: {}", lib_name.toStdString());
            return;
        }

        auto fn_get_instance = (FnGetInstance)rtc_lib_->resolve("GetInstance");
        if (!fn_get_instance) {
            LOGE("DON'T have GetInstance");
            return;
        }

        rtc_client_ = (RtcClientInterface*)fn_get_instance();
        if (!rtc_client_) {
            LOGE("Can't get rtc client instance.");
            return;
        }
        LOGI("Load rtc library success.");
#endif
    }

    void WebRtcLocalConnection::InitRtcClient() {
        if (!rtc_client_) {
            LOGE("Rtc client is null, init failed.");
            return;
        }

        // local(direct) mode: no STUN server, non-trickle,
        // the final offer sdp is reported after ice gathering complete
        rtc_client_->SetLocalRtcMode(true);

        rtc_client_->SetOnLocalSdpSetCallback([=, this](const std::string& sdp) {
            // called on the webrtc signaling thread, hand off to our own thread
            // before doing the blocking http request
            LOGI("Got the final offer sdp, size: {}, will request the answer by http.", sdp.size());
            RunInRtcThread([=, this]() {
                this->RequestAnswerSdp(sdp, false);
            });
        });

        rtc_client_->SetMediaMessageCallback([=, this](std::shared_ptr<Data> msg) {
            if (msg_cbk_) {
                msg_cbk_(msg);
            }
        });

        rtc_client_->SetFtMessageCallback([=, this](std::shared_ptr<Data> msg) {
            if (ft_msg_cbk_) {
                ft_msg_cbk_(msg);
            }
        });

        rtc_client_->SetOnVideoFrameCallback([=, this](int w, int h, std::shared_ptr<Data> i420) {
            if (video_frame_cbk_) {
                video_frame_cbk_(w, h, i420);
            }
        });

        rtc_client_->SetOnIceStateCallback([=, this](int state) {
            LOGI("Rtc local, ice state changed: {}", state);
            if (state == kIceStateConnected || state == kIceStateCompleted) {
                RunInRtcThread([=, this]() {
                    // ice connected doesn't mean the sctp data channels are open yet,
                    // wait for the media channel before reporting connected(hello goes through it)
                    for (int i = 0; i < 100 && !stopped_; ++i) {
                        if (this->IsMediaChannelReady()) {
                            break;
                        }
                        TimeUtil::DelayBySleep(50);
                    }
                    if (stopped_) {
                        return;
                    }
                    if (!connected_.exchange(true)) {
                        LOGI("Rtc local, connected.");
                        if (conn_cbk_) {
                            conn_cbk_();
                        }
                    }
                });
            }
            else if (state == kIceStateFailed || state == kIceStateDisconnected || state == kIceStateClosed) {
                if (connected_.exchange(false)) {
                    LOGW("Rtc local, disconnected, ice state: {}", state);
                    if (dis_conn_cbk_) {
                        dis_conn_cbk_();
                    }
                }
            }
        });

        if (!rtc_client_->Init(sdk_params_->bare_remote_device_id_)) {
            LOGE("Rtc local client init FAILED!");
            return;
        }
        LOGI("Rtc local client init success.");
    }

    void WebRtcLocalConnection::RequestAnswerSdp(const std::string& offer_sdp, bool takeover) {
        if (stopped_) {
            return;
        }

        std::map<std::string, std::string> query;
        query["device_id"] = sdk_params_->bare_remote_device_id_;
        query["stream_id"] = sdk_params_->stream_id_;
        auto pwd_md5 = MakeSafetyPwdMd5();
        if (!pwd_md5.empty()) {
            query["safety_pwd_md5"] = pwd_md5;
        }
        if (takeover) {
            query["takeover"] = "1";
        }

        nlohmann::json body;
        body["sdp"] = offer_sdp;

        auto client = HttpClient::Make(sdk_params_->ip_, sdk_params_->port_, "/alloc/local/rtc", 15000);
        auto resp = client->Post(query, body.dump(), "application/json");
        if (resp.status != 200) {
            LOGE("Request rtc local answer failed, http status: {}, error: {}", resp.status, resp.error_message);
            if (resp.status == 403 && msg_notifier_) {
                // wrong device password(random or safety) - let the UI tell the user instead of hanging
                msg_notifier_->SendAppMessage(SdkMsgRtcLocalAuthFailed{});
                this->Stop();
            }
            return;
        }

        int code = -1;
        std::string message;
        std::string answer_sdp;
        try {
            auto obj = nlohmann::json::parse(resp.body);
            code = obj.value("code", -1);
            message = obj.value("message", "");
            if (obj.contains("data") && obj["data"].is_object()) {
                answer_sdp = obj["data"].value("answer_sdp", "");
            }
        }
        catch (std::exception& e) {
            LOGE("Parse rtc local answer failed: {}, body: {}", e.what(), resp.body);
            return;
        }

        if (code == kRtcLocalRespOccupied && !takeover) {
            // occupied by another connection, retry once with takeover=1
            LOGW("Rtc local connection is occupied, retry with takeover=1.");
            this->RequestAnswerSdp(offer_sdp, true);
            return;
        }
        if (code != 200) {
            LOGE("Request rtc local answer rejected, code: {}, message: {}", code, message);
            return;
        }
        if (answer_sdp.empty()) {
            LOGE("Rtc local answer sdp is empty.");
            return;
        }

        LOGI("Got rtc local answer sdp, size: {}, will set remote desc.", answer_sdp.size());
        if (rtc_client_) {
            rtc_client_->OnRemoteSdp(answer_sdp);
        }
    }

    std::string WebRtcLocalConnection::MakeSafetyPwdMd5() {
        // priority: safety password(already in md5 form) > random password(plain, md5 it)
        if (!sdk_params_->remote_device_safety_pwd_.empty()) {
            return sdk_params_->remote_device_safety_pwd_;
        }
        if (!sdk_params_->remote_device_random_pwd_.empty()) {
            return MD5::Hex(sdk_params_->remote_device_random_pwd_);
        }
        return "";
    }

    void WebRtcLocalConnection::PostBinaryMessage(std::shared_ptr<Data> msg) {
        this->PostMediaMessage(msg);
    }

    void WebRtcLocalConnection::PostMediaMessage(std::shared_ptr<Data> msg) {
        if (!rtc_client_) {
            return;
        }
        // keyboard/mouse go through the dedicated input channel: the render dispatches
        // it via the direct fast path(CallbackEventDirectly) instead of queueing on the
        // plugin work thread. channel is reliable+ordered, no events are lost.
        if (msg && rtc_client_->IsInputChannelReady()) {
            tc::Message proto_msg;
            if (proto_msg.ParseFromArray(msg->DataAddr(), (int)msg->Size())) {
                const auto type = proto_msg.type();
                if (type == tc::kKeyEvent || type == tc::kMouseEvent) {
                    rtc_client_->PostInputMessage(msg);
                    return;
                }
            }
        }
        rtc_client_->PostMediaMessage(msg);
    }

    void WebRtcLocalConnection::PostFtMessage(std::shared_ptr<Data> msg) {
        if (rtc_client_) {
            rtc_client_->PostFtMessage(msg);
        }
    }

    void WebRtcLocalConnection::SetOnFtMessageCallback(const std::function<void(std::shared_ptr<Data>)>& cbk) {
        ft_msg_cbk_ = cbk;
    }

    void WebRtcLocalConnection::SetOnRtcVideoFrameCallback(const std::function<void(int w, int h, std::shared_ptr<Data> i420)>& cbk) {
        video_frame_cbk_ = cbk;
    }

    int64_t WebRtcLocalConnection::GetQueuingMsgCount() {
        return this->GetQueuingMediaMsgCount();
    }

    int64_t WebRtcLocalConnection::GetQueuingMediaMsgCount() {
        return rtc_client_ ? rtc_client_->GetQueuingMediaMsgCount() : 0;
    }

    int64_t WebRtcLocalConnection::GetQueuingFtMsgCount() {
        return rtc_client_ ? rtc_client_->GetQueuingFtMsgCount() : 0;
    }

    bool WebRtcLocalConnection::HasEnoughBufferForQueuingMediaMessages() {
        return rtc_client_ && rtc_client_->HasEnoughBufferForQueuingMediaMessages();
    }

    bool WebRtcLocalConnection::HasEnoughBufferForQueuingFtMessages() {
        return rtc_client_ && rtc_client_->HasEnoughBufferForQueuingFtMessages();
    }

    bool WebRtcLocalConnection::IsMediaChannelReady() {
        return rtc_client_ && rtc_client_->IsMediaChannelReady();
    }

    bool WebRtcLocalConnection::IsFtChannelReady() {
        return rtc_client_ && rtc_client_->IsFtChannelReady();
    }

    bool WebRtcLocalConnection::IsAlive() {
        return connected_.load();
    }

    void WebRtcLocalConnection::On16msTimeout() {
        if (rtc_client_) {
            rtc_client_->On16msTimeout();
        }
    }

    void WebRtcLocalConnection::RunInRtcThread(std::function<void()>&& task) {
        thread_->Post(std::move(task));
    }

}
