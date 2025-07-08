//
// Created by RGAA on 2024-04-21.
//

#include "sdk_statistics.h"
#include "tc_message.pb.h"
#include "tc_common_new/log.h"
#include "tc_common_new/num_formatter.h"

namespace tc
{

    SdkStatistics::SdkStatistics() {

    }

    void SdkStatistics::AppendDecodeDuration(const std::string& monitor_name, int32_t time) {
        if (!decode_durations_.HasKey(monitor_name)) {
            decode_durations_.Insert(monitor_name, {});
        }
        decode_durations_.VisitAllCond([&](std::string k, std::vector<float>& ds) -> bool {
            if (monitor_name == k) {
                if (ds.size() >= kMaxStatCounts) {
                    ds.erase(ds.begin());
                }
                ds.push_back((float)time);
                return true;
            }
            return false;
        });
    }

    std::map<std::string, std::vector<float>> SdkStatistics::GetDecodeDurations() {
        std::map<std::string, std::vector<float>> dss;
        decode_durations_.ApplyAll([&](const std::string& k, const std::vector<float>& ds) {
            dss.insert({k, ds});
        });
        return dss;
    }

    void SdkStatistics::AppendVideoRecvGap(const std::string& monitor_name, int32_t time) {
        if (!video_recv_gaps_.HasKey(monitor_name)) {
            video_recv_gaps_.Insert(monitor_name, {});
        }
        video_recv_gaps_.VisitAllCond([&](std::string k, std::vector<float>& gaps) -> bool {
            if (monitor_name == k) {
                if (gaps.size() >= kMaxStatCounts) {
                    gaps.erase(gaps.begin());
                }
                gaps.push_back(time);
                return true;
            }
            return false;
        });
    }

    std::map<std::string, std::vector<float>> SdkStatistics::GetVideoRecvGaps() {
        std::map<std::string, std::vector<float>> gaps;
        video_recv_gaps_.ApplyAll([&](const std::string& k, const std::vector<float>& ds) {
            gaps.insert({k, ds});
        });
        return gaps;
    }

    void SdkStatistics::AppendRecvDataSize(int size) {
        recv_data_size_ += size;
    }

    void SdkStatistics::AppendSentDataSize(int size) {
        send_data_size_  += size;
    }

    void SdkStatistics::AppendNetTimeDelay(int32_t delay) {
        if (net_delays_.Size() >= kMaxStatCounts) {
            net_delays_.RemoveFirst();
        }
        net_delays_.PushBack(delay);
    }

    void SdkStatistics::TickVideoRecvFps(const std::string& monitor_name) {
        if (!fps_video_recv_.HasKey(monitor_name)) {
            fps_video_recv_.Insert(monitor_name, std::make_shared<FpsStat>());
        }
        fps_video_recv_.Get(monitor_name)->Tick();
    }

    void SdkStatistics::TickFrameRenderFps(const std::string& monitor_name) {
        if (!fps_render_.HasKey(monitor_name)) {
            fps_render_.Insert(monitor_name, std::make_shared<FpsStat>());
        }
        fps_render_.Get(monitor_name)->Tick();
    }

    void SdkStatistics::UpdateFrameSize(const std::string& monitor_name, int width, int height) {
        if (!frames_size_.HasKey(monitor_name)) {
            frames_size_.Insert(monitor_name, SdkStatFrameSize {
                .width_ = width,
                .height_ = height,
            });
        }
        else {
            frames_size_.VisitAllCond([&](std::string k, auto& size) {
                if (k == monitor_name) {
                    size.width_ = width;
                    size.height_ = height;
                    return true;
                }
                return false;
            });
        }
    }

    std::map<std::string, SdkStatFrameSize> SdkStatistics::GetFramesSize() {
        std::map<std::string, SdkStatFrameSize> r;
        frames_size_.VisitAll([&](auto k, auto& v) {
            r.insert({k, v});
        });
        return r;
    }

    void SdkStatistics::CalculateDataSpeed() {
        if (recv_data_size_ >= last_recv_data_size_) {
            auto diff = (recv_data_size_ - last_recv_data_size_)*1.0;
            diff /= (1024*1024);
            last_recv_data_size_ = recv_data_size_.load();
            recv_data_speeds_.PushBack(NumFormatter::Round2DecimalPlaces((float)diff));
            if (recv_data_speeds_.Size() > kMaxStatCounts) {
                recv_data_speeds_.RemoveFirst();
            }
        }
        if (send_data_size_ >= last_send_data_size_) {
            auto diff = (send_data_size_ - last_send_data_size_)*1.0;
            diff /= (1024*1024);
            last_send_data_size_ = send_data_size_.load();
            send_data_speeds_.PushBack(NumFormatter::Round2DecimalPlaces((float)diff));
            if (send_data_speeds_.Size() > kMaxStatCounts) {
                send_data_speeds_.RemoveFirst();
            }
        }
    }

    void SdkStatistics::CalculateVideoFrameFps() {
        std::map<std::string, int> monitor_fps;
        fps_video_recv_.VisitAll([&](auto mon_name, auto& fps_stat) {
            auto value = fps_stat->value();
            monitor_fps.insert({mon_name, value});
        });

        for (const auto& [mon_name, value] : monitor_fps) {
            if (!video_recv_fps_.HasKey(mon_name)) {
                video_recv_fps_.Insert(mon_name, {});
            }
            video_recv_fps_.VisitAllCond([&](std::string k, std::vector<float>& fps) -> bool {
                if (mon_name == k) {
                    fps.push_back(value);
                    if (fps.size() > kMaxStatCounts) {
                        fps.erase(fps.begin());
                    }
                    return true;
                }
                return false;
            });
        }
    }

    std::map<std::string, std::vector<float>> SdkStatistics::GetVideoRecvFps() {
        std::map<std::string, std::vector<float>> r;
        video_recv_fps_.VisitAll([&](auto k, auto& v) {
            r.insert({k, v});
        });
        return r;
    }

    std::vector<float> SdkStatistics::GetRecvDataSpeeds() {
        std::vector<float> speeds;
        recv_data_speeds_.Visit([&](auto speed) {
            speeds.push_back(speed);
        });
        return speeds;
    }

    std::vector<float> SdkStatistics::GetSendDataSpeeds() {
        std::vector<float> speeds;
        send_data_speeds_.Visit([&](auto speed) {
            speeds.push_back(speed);
        });
        return speeds;
    }

    std::vector<float> SdkStatistics::GetNetDelays() {
        std::vector<float> delays;
        net_delays_.Visit([&](auto delay) {
            delays.push_back(delay);
        });
        return delays;
    }

    std::map<std::string, IsolatedMonitorStatisticsInfoInRender> SdkStatistics::GetRenderMonitorsStat() {
        std::map<std::string, IsolatedMonitorStatisticsInfoInRender> stats;
        render_monitor_stat_.VisitAll([&](auto k, auto& v) {
            stats.insert({k, v});
        });
        return stats;
    }

    void SdkStatistics::UpdateIsolatedMonitorStatisticsInfoInRender(const std::string& mon_name, const IsolatedMonitorStatisticsInfoInRender& info) {
        render_monitor_stat_.Replace(mon_name, info);
    }

}
