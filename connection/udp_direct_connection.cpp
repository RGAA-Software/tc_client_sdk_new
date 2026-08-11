//
// Created by RGAA on 12/08/2026.
//

#include "udp_direct_connection.h"
#include "tc_common_new/log.h"
#include "tc_common_new/data.h"
#include "tc_common_new/time_util.h"
#include "tc_message.pb.h"
#include <asio2/asio2.hpp>
#include <asio2/udp/udp_client.hpp>

namespace tc
{

    UdpDirectConnection::UdpDirectConnection(const std::shared_ptr<ThunderSdkParams>& params,
                                             const std::shared_ptr<MessageNotifier>& notifier)
                                             : Connection(params, notifier) {
        // 组帧完成:合成标准 kVideoFrame proto 上送(走与 webrtc_local 相同的回调通道,不回 Ack)
        reassembler_.on_frame_ = [this](const GrUdpFrameReassembler::CompleteFrame& frame) {
            this->OnCompleteFrame(frame);
        };
        // 判丢:请 render 重发 IDR(空 mon_name = 所有屏)
        reassembler_.on_frame_lost_ = [this](uint8_t mon_slot, uint32_t lost_frame_index) {
            LOGW("Udp direct frame lost, mon slot: {}, frame: {}, request IDR.", mon_slot, lost_frame_index);
            this->RequestIdr("");
        };
    }

    void UdpDirectConnection::Start(const std::string& host, int udp_port,
                                    const std::string& device_id, const std::string& stream_id) {
        host_ = host;
        udp_port_ = udp_port;
        device_id_ = device_id;
        stream_id_ = stream_id;

        udp_client_ = std::make_shared<asio2::udp_client>();
        // 注意:裸 UDP,不传 asio2::use_kcp(可靠重传对视频是负优化,见 udp_gamestream_channel_plan.md)

        udp_client_->bind_connect([this]() {
            if (asio2::get_last_error()) {
                LOGE("udp direct connect failure : {} {}", asio2::last_error_val(), asio2::last_error_msg().c_str());
            }
            else {
                LOGI("udp direct connect success : {} {}, remote: {}:{}", udp_client_->local_address().c_str(),
                     udp_client_->local_port(), host_, udp_port_);
                connected_ = true;
                last_recv_ms_ = TimeUtil::GetCurrentTimestamp();
                // 立即发 hello,render 按源地址绑定媒体会话并触发该屏 IDR
                this->PostBinaryMessage(GrUdpProtocol::BuildHello(device_id_, stream_id_));
                // 1s 心跳:保持 NAT 映射,让 render 感知会话在线
                udp_client_->start_timer(kTimerHeartbeat, 1000, [this]() {
                    this->PostBinaryMessage(GrUdpProtocol::BuildHeartbeat(stream_id_));
                });
                // watchdog:长时间收不到任何 UDP 包视为媒体面断开
                udp_client_->start_timer(kTimerWatchdog, 1000, [this]() {
                    this->CheckWatchdog();
                });
                udp_client_->post_queued_event([this]() {
                    if (conn_cbk_) {
                        conn_cbk_();
                    }
                });
            }

        }).bind_disconnect([this]() {
            LOGI("udp direct disconnect : {} {}", asio2::last_error_val(), asio2::last_error_msg().c_str());
            connected_ = false;
            if (!disconn_reported_.exchange(true) && dis_conn_cbk_) {
                dis_conn_cbk_();
            }

        }).bind_recv([this](std::string_view data) {
            this->OnUdpPacket(data.data(), data.size());
        });

        udp_client_->async_start(host_, udp_port_);
    }

    void UdpDirectConnection::Stop() {
        if (stopped_.exchange(true)) {
            return;
        }
        if (udp_client_ && udp_client_->is_started()) {
            udp_client_->stop_all_timers();
            udp_client_->stop();
        }
    }

    void UdpDirectConnection::PostBinaryMessage(std::shared_ptr<Data> msg) {
        if (udp_client_ && udp_client_->is_started()) {
            queuing_message_count_++;
            udp_client_->async_send(msg->CStr(), msg->Size(), [this]() {
                queuing_message_count_--;
            });
        }
    }

    void UdpDirectConnection::OnUdpPacket(const char* data, size_t size) {
        last_recv_ms_ = TimeUtil::GetCurrentTimestamp();
        auto pkt_type = GrUdpProtocol::ParseCommon(data, size);
        if (pkt_type == GrUdpProtocol::kPktVideo) {
            reassembler_.AddPacket(data, size);
        }
        else if (pkt_type == GrUdpProtocol::kPktCtrl) {
            std::string s1, s2;
            auto subtype = GrUdpProtocol::ParseCtrl(data, size, s1, s2);
            if (subtype == GrUdpProtocol::kCtrlKick) {
                LOGW("Udp direct kicked by render, reason: {}", s1);
                if (on_kick_cbk_) {
                    on_kick_cbk_(s1);
                }
            }
        }
        // 音频(kPktAudio)P2 再接入,这里先忽略
    }

    void UdpDirectConnection::OnCompleteFrame(const GrUdpFrameReassembler::CompleteFrame& frame) {
        if (stopped_ || !video_msg_cbk_ || !frame.data_ || frame.data_->Size() == 0) {
            return;
        }

        // 合成与 relay/ws 路径完全一致的标准 kVideoFrame proto,
        // 让 sdk 的按屏解码链原样接上(reassembler 保证首帧必为 IDR)
        auto msg = std::make_shared<tc::Message>();
        msg->set_type(tc::kVideoFrame);
        auto* video = msg->mutable_video_frame();
        video->set_type(frame.codec_ == GrUdpProtocol::kCodecH265 ? tc::kNetHevc : tc::kNetH264);
        video->set_data(frame.data_->CStr(), frame.data_->Size());
        video->set_frame_index(frame.frame_index_);
        video->set_key(frame.key_);
        video->set_frame_width(frame.frame_width_);
        video->set_frame_height(frame.frame_height_);
        video->set_mon_name(frame.mon_name_);
        video->set_mon_index(frame.mon_slot_);
        // debug 标记:区分 UDP 合成帧与其它 kVideoFrame 来源(参照 webrtc_local 的 rtc_synth)
        video->set_extra("udp_synth");

        video_msg_cbk_(msg);
    }

    void UdpDirectConnection::RequestIdr(const std::string& mon_name) {
        this->PostBinaryMessage(GrUdpProtocol::BuildIdrRequest(mon_name));
    }

    void UdpDirectConnection::CheckWatchdog() {
        if (stopped_ || !connected_) {
            return;
        }
        auto idle = TimeUtil::GetCurrentTimestamp() - last_recv_ms_.load();
        if (idle > kWatchdogTimeoutMs) {
            LOGW("Udp direct watchdog timeout, no udp packet for {}ms, report disconnected.", idle);
            connected_ = false;
            if (!disconn_reported_.exchange(true) && dis_conn_cbk_) {
                dis_conn_cbk_();
            }
        }
    }

    void UdpDirectConnection::SetOnVideoMessageCallback(const std::function<void(std::shared_ptr<tc::Message>)>& cbk) {
        video_msg_cbk_ = cbk;
    }

    void UdpDirectConnection::SetOnKickCallback(std::function<void(const std::string& reason)> cbk) {
        on_kick_cbk_ = std::move(cbk);
    }

    bool UdpDirectConnection::IsAlive() {
        return connected_.load();
    }

}
