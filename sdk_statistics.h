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

    class SdkStatistics {
    public:

        static SdkStatistics* Instance() {
            static SdkStatistics instance;
            return &instance;
        }

        SdkStatistics();

        void AppendDecodeDuration(uint32_t time);
        void AppendVideoRecvGap(uint32_t time);
        void AppendMediaDataSize(int size);

        void TickFps();
        std::string AsProtoMessage(const std::string& device_id, const std::string& stream_id);

        void Dump();

    public:
        std::vector<uint32_t> decode_durations_;
        std::vector<uint32_t> video_recv_gaps_;
        std::shared_ptr<FpsStat> fps_video_recv_ = nullptr;
        int fps_video_recv_value_ = 0;
        std::shared_ptr<FpsStat> fps_render_ = nullptr;
        int fps_render_value_ = 0;
        std::atomic_int64_t recv_media_data_ = 0;
        int render_width_ = 0;
        int render_height_ = 0;
    };

}

#endif //GAMMARAYPC_STATISTICS_H
