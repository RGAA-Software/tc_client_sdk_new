//
// Created by RGAA on 2024-04-21.
//

#ifndef GAMMARAYPC_STATISTICS_H
#define GAMMARAYPC_STATISTICS_H

#include <vector>
#include <cstdint>
#include <string>
#include <atomic>
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
        void AppendMediaDataSize(int size);

        std::string AsProtoMessage();

        void Dump();

    public:
        std::vector<uint32_t> decode_durations_;
        std::vector<uint32_t> video_recv_gaps_;
        std::shared_ptr<FpsStat> fps_video_recv_ = nullptr;
        std::shared_ptr<FpsStat> fps_render_ = nullptr;
        std::atomic_int64_t recv_media_data_ = 0;
        int render_width_ = 0;
        int render_height_ = 0;
    };

}

#endif //GAMMARAYPC_STATISTICS_H