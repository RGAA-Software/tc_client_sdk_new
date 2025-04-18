//
// Created by RGAA on 16/04/2025.
//

#include "webrtc_connection.h"
#include "ct_rtc_manager.h"
#include "tc_webrtc_client/rtc_client_interface.h"

namespace tc
{

    WebRtcConnection::WebRtcConnection(const std::shared_ptr<RelayConnection>& relay_conn,
                                       const std::shared_ptr<ThunderSdkParams>& params,
                                       const std::shared_ptr<MessageNotifier>& notifier)
                                       : Connection(params, notifier) {
        this->relay_conn_ = relay_conn;
        this->rtc_mgr_ = std::make_shared<CtRtcManager>(relay_conn, params, notifier);
    }

    WebRtcConnection::~WebRtcConnection() {

    }

    void WebRtcConnection::Start() {

    }

    void WebRtcConnection::Stop() {

    }

    void WebRtcConnection::PostBinaryMessage(const std::string& msg) {
        rtc_mgr_->PostMediaMessage(msg);
    }

    void WebRtcConnection::PostMediaMessage(const std::string& msg) {
        rtc_mgr_->PostMediaMessage(msg);
    }

    void WebRtcConnection::PostFtMessage(const std::string& msg) {
        rtc_mgr_->PostFtMessage(msg);
    }

    void WebRtcConnection::SetOnMediaMessageCallback(const std::function<void(const std::string&)>& cbk) {
        rtc_mgr_->SetOnMediaMessageCallback(cbk);
    }

    void WebRtcConnection::SetOnFtMessageCallback(const std::function<void(const std::string&)>& cbk) {
        rtc_mgr_->SetOnFtMessageCallback(cbk);
    }

    int64_t WebRtcConnection::GetQueuingMsgCount() {
        return 0;
    }

    int64_t WebRtcConnection::GetQueuingMediaMsgCount() {
        return rtc_mgr_->GetQueuingMediaMsgCount();
    }

    int64_t WebRtcConnection::GetQueuingFtMsgCount() {
        return rtc_mgr_->GetQueuingFtMsgCount();
    }

    void WebRtcConnection::RequestPauseStream() {

    }

    void WebRtcConnection::RequestResumeStream() {

    }

    bool WebRtcConnection::HasEnoughBufferForQueuingMediaMessages() {
        return rtc_mgr_->GetRtcClient()->HasEnoughBufferForQueuingMediaMessages();
    }

    bool WebRtcConnection::HasEnoughBufferForQueuingFtMessages() {
        return rtc_mgr_->GetRtcClient()->HasEnoughBufferForQueuingFtMessages();
    }

}