//
// Created by RGAA on 2024-04-21.
//

#ifndef GAMMARAYPC_STATISTICS_H
#define GAMMARAYPC_STATISTICS_H

#include <map>
#include <vector>
#include <cstdint>
#include <string>
#include <atomic>
#include "tc_common_new/fps_stat.h"
#include "tc_message.pb.h"

namespace tc
{

    constexpr auto kMaxStatCounts = 180;

    class SdkStatFrameSize {
    public:
        int width_ = 0;
        int height_ = 0;
    };

    class SdkStatistics {
    public:

        static SdkStatistics* Instance() {
            static SdkStatistics instance;
            return &instance;
        }

        SdkStatistics();

        void AppendDecodeDuration(const std::string& monitor_name, int32_t time);
        void AppendVideoRecvGap(const std::string& monitor_name, int32_t time);
        void AppendDataSize(int size);
        void AppendNetTimeDelay(int32_t delay);

        void TickVideoRecvFps(const std::string& monitor_name);
        void TickFrameRenderFps(const std::string& monitor_name);
        void UpdateFrameSize(const std::string& monitor_name, int width, int height);
        // By one second
        void CalculateDataSpeed();
        void CalculateVideoFrameFps();

        std::string AsProtoMessage(const std::string& device_id, const std::string& stream_id);

        void Dump();

    private:
        std::map<std::string, std::shared_ptr<FpsStat>> fps_video_recv_;
        std::map<std::string, std::shared_ptr<FpsStat>> fps_render_;

    public:
        // monitor name <==> value
        std::map<std::string, std::vector<float>> decode_durations_;
        std::map<std::string, std::vector<float>> video_recv_gaps_;
        std::map<std::string, std::vector<float>> video_recv_fps_;
        std::map<std::string, SdkStatFrameSize> frames_size_;
        std::atomic_int64_t recv_data_ = 0;
        int64_t last_recv_data_ = 0;

        // h264 / hevc ...
        std::string video_format_;

        // 4:2:0 / 4:4:4
        std::string video_color_;

        //
        std::string video_decoder_;

        // in MB/S
        std::vector<float> data_speeds_;

        // in ms
        std::vector<float> net_delays_;

        //
        std::map<std::string, IsolatedMonitorStatisticsInfoInRender> render_monitor_stat_;

        // DXGI / GDI
        std::string video_capture_type_;

        // WASAPI / Inner[HOOK]
        std::string audio_capture_type_;

        // OPUS / ...
        std::string audio_encode_type_;

        // remote pc info
        std::string remote_pc_info_;

        // remote desktop name
        std::string remote_desktop_name_;

        // remote os name
        std::string remote_os_name_;

        // remote detailed hardware information
        tc::RdHardwareInfo remote_hd_info_;
    };

}

#endif //GAMMARAYPC_STATISTICS_H
