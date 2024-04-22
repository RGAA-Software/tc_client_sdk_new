//
// Created by RGAA on 2024-04-21.
//

#ifndef GAMMARAYPC_STATISTICS_H
#define GAMMARAYPC_STATISTICS_H

#include <vector>
#include <cstdint>
#include <string>
#include "tc_common_new/fps_stat.h"

namespace tc
{

    constexpr auto kMaxStatCounts = 180;

    class Statistics {
    public:

        static Statistics* Instance() {
            static Statistics instance;
            return &instance;
        }

        Statistics();

        void AppendDecodeDuration(uint32_t time);
        void AppendVideoRecvGap(uint32_t time);
        std::string AsProtoMessage();

        void Dump();

    public:
        std::vector<uint32_t> decode_durations_;
        std::vector<uint32_t> video_recv_gaps_;
        std::shared_ptr<FpsStat> fps_video_recv_ = nullptr;
    };

}

#endif //GAMMARAYPC_STATISTICS_H
