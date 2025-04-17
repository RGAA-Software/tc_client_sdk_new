//
// Created by RGAA on 13/04/2025.
//

#ifndef GAMMARAY_CT_RTC_MANAGER_H
#define GAMMARAY_CT_RTC_MANAGER_H

#include <memory>
#include <QLibrary>
#include <QApplication>
#include "sdk_messages.h"
#include "sdk_params.h"

namespace tc
{

    class Thread;
    class MessageNotifier;
    class RtcClientInterface;
    class Message;
    class MessageListener;
    class RelayConnection;

    class CtRtcManager {
    public:
        CtRtcManager(const std::shared_ptr<RelayConnection>& relay_conn,
                     const std::shared_ptr<ThunderSdkParams>& params,
                     const std::shared_ptr<MessageNotifier>& notifier);

        void PostMediaMessage(const std::string& msg);
        void PostFtMessage(const std::string& msg);

        void SetOnMediaMessageCallback(const std::function<void(const std::string&)>&);
        void SetOnFtMessageCallback(const std::function<void(const std::string&)>&);

    private:
        void Init();
        void LoadRtcLibrary();
        void OnRemoteSdp(const SdkMsgRemoteAnswerSdp& m);
        void OnRemoteIce(const SdkMsgRemoteIce& m);

        void SendSdpToRemote(const std::string& sdp);
        void SendIceToRemote(const std::string& ice, const std::string& mid, int sdp_mline_index);

        void RunInRtcThread(std::function<void()>&&);

    private:
        std::shared_ptr<ThunderSdkParams> sdk_params_;
        std::shared_ptr<Thread> thread_ = nullptr;
        QLibrary* rtc_lib_ = nullptr;
        RtcClientInterface* rtc_client_ = nullptr;
        std::shared_ptr<MessageListener> msg_listener_ = nullptr;
        std::shared_ptr<MessageNotifier> msg_notifier_ = nullptr;
        std::shared_ptr<RelayConnection> relay_conn_;

        std::function<void(const std::string&)> media_msg_cbk_;
        std::function<void(const std::string&)> ft_msg_cbk_;
    };

}

#endif //GAMMARAY_CT_RTC_MANAGER_H
