//
// Created by RGAA on 2024-04-21.
//

#include "statistics.h"
#include "tc_message.pb.h"

namespace tc
{

    void Statistics::AppendDecodeDuration(uint32_t time) {
        if (decode_durations_.size() >= kMaxStatCounts) {
            decode_durations_.erase(decode_durations_.begin());
        }
        decode_durations_.push_back(time);
    }

    std::string Statistics::AsProtoMessage() {
        tc::Message msg;
        msg.set_type(tc::MessageType::kClientStatistics);
        auto cst = msg.mutable_client_statistics();
        cst->mutable_decode_durations()->Add(decode_durations_.begin(), decode_durations_.end());
        return msg.SerializeAsString();
    }

}
