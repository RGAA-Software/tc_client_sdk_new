//
// Created by RGAA on 8/12/2024.
//

#include "connection.h"

namespace tc
{

    Connection::Connection(const std::shared_ptr<MessageNotifier>& notifier) {
        msg_notifier_ = notifier;
    }

    Connection::~Connection() {

    }

    void Connection::Start() {

    }

    void Connection::Stop() {

    }

    int64_t Connection::GetQueuingMsgCount() {
        return queuing_message_count_;
    }

}