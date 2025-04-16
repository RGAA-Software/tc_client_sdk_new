//
// Created by RGAA on 16/04/2025.
//

#ifndef GAMMARAY_WEBRTC_CONNECTION_H
#define GAMMARAY_WEBRTC_CONNECTION_H

#include <functional>
#include <string>
#include "tc_client_sdk_new/connection/connection.h"

namespace tc
{

    // we need relay to exchange signaling messages
    class RelayConnection;
    class CtRtcManager;

    class WebRtcConnection : public Connection {
    public:
        explicit WebRtcConnection(const std::shared_ptr<RelayConnection>& relay_conn,
                                  const ThunderSdkParams& params,
                                  const std::shared_ptr<MessageNotifier>& notifier);
        ~WebRtcConnection();

        void Start() override;
        void Stop() override;
        // @see PostMediaMessage
        void PostBinaryMessage(const std::string& msg) override;

        void PostMediaMessage(const std::string& msg);
        void PostFtMessage(const std::string& msg);
        //
        void SetOnMediaMessageCallback(const std::function<void(const std::string&)>&);
        void SetOnFtMessageCallback(const std::function<void(const std::string&)>&);

        int64_t GetQueuingMsgCount() override;
        void RequestPauseStream() override;
        void RequestResumeStream() override;

    private:
        std::shared_ptr<RelayConnection> relay_conn_ = nullptr;
        std::shared_ptr<CtRtcManager> rtc_mgr_ = nullptr;
    };

}

#endif //GAMMARAY_WEBRTC_CONNECTION_H
