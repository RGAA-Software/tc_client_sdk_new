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
        if (!decode_durations_.contains(monitor_name)) {
            decode_durations_[monitor_name] = {};
        }
        auto& durations = decode_durations_[monitor_name];
        if (durations.size() >= kMaxStatCounts) {
            durations.erase(durations.begin());
        }
        durations.push_back(time);
    }

    void SdkStatistics::AppendVideoRecvGap(const std::string& monitor_name, int32_t time) {
        if (!video_recv_gaps_.contains(monitor_name)) {
            video_recv_gaps_[monitor_name] = {};
        }
        auto& gaps = video_recv_gaps_[monitor_name];
        if (gaps.size() >= kMaxStatCounts) {
            gaps.erase(gaps.begin());
        }
        gaps.push_back(time);
    }

    void SdkStatistics::AppendRecvDataSize(int size) {
        recv_data_size_ += size;
    }

    void SdkStatistics::AppendSentDataSize(int size) {
        send_data_size_  += size;
    }

    void SdkStatistics::AppendNetTimeDelay(int32_t delay) {
        if (net_delays_.size() >= kMaxStatCounts) {
            net_delays_.erase(net_delays_.begin());
        }
        net_delays_.push_back(delay);
    }

    void SdkStatistics::TickVideoRecvFps(const std::string& monitor_name) {
        if (!fps_video_recv_.contains(monitor_name)) {
            fps_video_recv_[monitor_name] = std::make_shared<FpsStat>();
        }
        fps_video_recv_[monitor_name]->Tick();
    }

    void SdkStatistics::TickFrameRenderFps(const std::string& monitor_name) {
        if (!fps_render_.contains(monitor_name)) {
            fps_render_[monitor_name] = std::make_shared<FpsStat>();
        }
        fps_render_[monitor_name]->Tick();
    }

    void SdkStatistics::UpdateFrameSize(const std::string& monitor_name, int width, int height) {
        if (!frames_size_.contains(monitor_name)) {
            frames_size_.insert({monitor_name, SdkStatFrameSize {
                .width_ = width,
                .height_ = height,
            }});
        }
        else {
            frames_size_[monitor_name].width_ = width;
            frames_size_[monitor_name].height_ = height;
        }
    }

    void SdkStatistics::CalculateDataSpeed() {
        if (recv_data_size_ >= last_recv_data_size_) {
            auto diff = (recv_data_size_ - last_recv_data_size_)*1.0;
            diff /= (1024*1024);
            last_recv_data_size_ = recv_data_size_.load();
            recv_data_speeds_.push_back(NumFormatter::Round2DecimalPlaces((float)diff));
            if (recv_data_speeds_.size() > kMaxStatCounts) {
                recv_data_speeds_.erase(recv_data_speeds_.begin());
            }
        }
        if (send_data_size_ >= last_send_data_size_) {
            auto diff = (send_data_size_ - last_send_data_size_)*1.0;
            diff /= (1024*1024);
            last_send_data_size_ = send_data_size_.load();
            send_data_speeds_.push_back(NumFormatter::Round2DecimalPlaces((float)diff));
            if (send_data_speeds_.size() > kMaxStatCounts) {
                send_data_speeds_.erase(send_data_speeds_.begin());
            }
        }
    }

    void SdkStatistics::CalculateVideoFrameFps() {
        for (const auto& [mon_name, fps_stat] : fps_video_recv_) {
            auto value = fps_stat->value();
            if (!video_recv_fps_.contains(mon_name)) {
                video_recv_fps_[mon_name] = {};
            }
            auto& target_video_recv_fps = video_recv_fps_[mon_name];
            target_video_recv_fps.push_back(value);
            if (target_video_recv_fps.size() > kMaxStatCounts) {
                target_video_recv_fps.erase(target_video_recv_fps.begin());
            }
        }
    }

    std::string SdkStatistics::AsProtoMessage(const std::string& device_id, const std::string& stream_id) {
        tc::Message msg;
        msg.set_type(tc::MessageType::kClientStatistics);
        msg.set_device_id(device_id);
        msg.set_stream_id(stream_id);
        auto cst = msg.mutable_client_statistics();
        //cst->mutable_decode_durations()->Add(decode_durations_.begin(), decode_durations_.end());
        //cst->mutable_video_recv_gaps()->Add(video_recv_gaps_.begin(), video_recv_gaps_.end());
        //cst->set_fps_video_recv(fps_video_recv_value_);
        //cst->set_fps_render(fps_render_value_);
        cst->set_recv_media_data(recv_data_size_);
        //cst->set_render_width(render_width_);
        //cst->set_render_height(render_height_);
        return msg.SerializeAsString();
    }

    void SdkStatistics::Dump() {
        LOGI("-------------------------SdkStatistics Begin-------------------------");
        //LOGI("Video recv fps: {}", fps_video_recv_value_);
        //LOGI("Frame render fps: {}", fps_render_value_);
        LOGI("Received data size: {} MB", recv_data_size_/1024/1024);
        LOGI("-------------------------SdkStatistics End---------------------------");
    }

}
