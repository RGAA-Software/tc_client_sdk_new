//
// Created by RGAA on 2024-04-21.
//

#include "statistics.h"
#include "tc_message.pb.h"
#include "tc_common_new/log.h"

namespace tc
{

    Statistics::Statistics() {
        fps_video_recv_ = std::make_shared<FpsStat>();
        fps_render_ = std::make_shared<FpsStat>();
    }

    void Statistics::AppendDecodeDuration(uint32_t time) {
        if (decode_durations_.size() >= kMaxStatCounts) {
            decode_durations_.erase(decode_durations_.begin());
        }
        decode_durations_.push_back(time);
    }

    void Statistics::AppendVideoRecvGap(uint32_t time) {
        if (video_recv_gaps_.size() >= kMaxStatCounts) {
            video_recv_gaps_.erase(video_recv_gaps_.begin());
        }
        video_recv_gaps_.push_back(time);
    }

    void Statistics::AppendMediaDataSize(int size) {
        recv_media_data_ += size;
    }

    void Statistics::TickFps() {
        fps_video_recv_value_ = fps_video_recv_->value();
        fps_render_value_ = fps_render_->value();
    }

    std::string Statistics::AsProtoMessage(const std::string& device_id, const std::string& stream_id) {
        tc::Message msg;
        msg.set_type(tc::MessageType::kClientStatistics);
        msg.set_device_id(device_id);
        msg.set_stream_id(stream_id);
        auto cst = msg.mutable_client_statistics();
        cst->mutable_decode_durations()->Add(decode_durations_.begin(), decode_durations_.end());
        cst->mutable_video_recv_gaps()->Add(video_recv_gaps_.begin(), video_recv_gaps_.end());
        cst->set_fps_video_recv(fps_video_recv_value_);
        cst->set_fps_render(fps_render_value_);
        cst->set_recv_media_data(recv_media_data_);
        cst->set_render_width(render_width_);
        cst->set_render_height(render_height_);
        return msg.SerializeAsString();
    }

    void Statistics::Dump() {
        LOGI("-------------------------Statistics Begin-------------------------");
        LOGI("Video recv fps: {}", fps_video_recv_value_);
        LOGI("Frame render fps: {}", fps_render_value_);
        LOGI("Received data size: {} MB", recv_media_data_/1024/1024);
        LOGI("-------------------------Statistics End---------------------------");
    }

}
