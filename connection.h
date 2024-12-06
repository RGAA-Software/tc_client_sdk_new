//
// Created by RGAA on 8/12/2024.
//

#ifndef GAMMARAYPC_CONNECTION_H
#define GAMMARAYPC_CONNECTION_H

#include <string>
#include <functional>

namespace tc
{

    using OnConnectedCallback = std::function<void()>;
    using OnDisConnectedCallback = std::function<void()>;
    using OnMessageCallback = std::function<void(std::string&&)>;

    class Connection {
    public:
        Connection();
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

    protected:
        OnConnectedCallback conn_cbk_;
        OnDisConnectedCallback dis_conn_cbk_;
        OnMessageCallback msg_cbk_;

    };

}

#endif //GAMMARAYPC_CONNECTION_H
