//
// Created by RGAA on 8/12/2024.
//

#ifndef GAMMARAYPC_CONNECTION_H
#define GAMMARAYPC_CONNECTION_H

#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include "sdk_params.h"

namespace tc
{

    using OnConnectedCallback = std::function<void()>;
    using OnDisConnectedCallback = std::function<void()>;
    using OnMessageCallback = std::function<void(std::string&&)>;

    class MessageNotifier;

    class Connection {
    public:
        Connection(const ThunderSdkParams& params, const std::shared_ptr<MessageNotifier>& notifier);
        ~Connection();

        void RegisterOnConnectedCallback(OnConnectedCallback&& cbk) {
            conn_cbk_ = cbk;
        }

        void RegisterOnDisConnectedCallback(OnDisConnectedCallback&& cbk) {
            dis_conn_cbk_ = cbk;
        }

        void RegisterOnMessageCallback(OnMessageCallback&& cbk) {
            msg_cbk_ = cbk;
        }

        virtual void Start();
        virtual void Stop();
        virtual void PostBinaryMessage(const std::string& msg) = 0;
        virtual void PostTextMessage(const std::string& msg) {}
        virtual int64_t GetQueuingMsgCount();
        virtual void RequestPauseStream() {}
        virtual void RequestResumeStream() {}

    protected:
        OnConnectedCallback conn_cbk_;
        OnDisConnectedCallback dis_conn_cbk_;
        OnMessageCallback msg_cbk_;
        std::atomic_int64_t queuing_message_count_ = 0;
        std::shared_ptr<MessageNotifier> msg_notifier_ = nullptr;
        ThunderSdkParams sdk_params_;
    };

}

#endif //GAMMARAYPC_CONNECTION_H
