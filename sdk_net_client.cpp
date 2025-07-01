//
// Created by RGAA on 2023-12-27.
//

#include "sdk_net_client.h"

#include <utility>
#include "tc_common_new/log.h"
#include "tc_common_new/data.h"
#include "tc_common_new/thread.h"
#include "tc_common_new/file.h"
#include "tc_common_new/message_notifier.h"
#include "sdk_messages.h"
#include "connection/udp_connection.h"
#include "connection/ws_connection.h"
#include "connection/relay_connection.h"
#include "connection/webrtc_connection.h"
#include "tc_common_new/time_util.h"
#include "sdk_statistics.h"

#include <asio2/websocket/ws_client.hpp>
#include <asio2/asio2.hpp>

namespace tc
{

    NetClient::NetClient(const std::shared_ptr<ThunderSdkParams>& params,
                         const std::shared_ptr<MessageNotifier>& notifier,
                         const std::string& ip,
                         int port,
                         const std::string& media_path,
                         const std::string& ft_path,
                         const ClientNetworkType& nt_type,
                         const std::string& device_id,
                         const std::string& remote_device_id,
                         const std::string& ft_device_id,
                         const std::string& ft_remote_device_id,
                         const std::string& stream_id) {

        this->stat_ = SdkStatistics::Instance();

        this->sdk_params_ = params;
        this->msg_notifier_ = notifier;
        this->ip_ = ip;
        this->port_ = port;
        this->media_path_ = media_path;
        this->ft_path_ = ft_path;
        this->network_type_ = nt_type;
        this->device_id_ = device_id;
        this->remote_device_id_ = remote_device_id;
        this->ft_device_id_ = ft_device_id;
        this->ft_remote_device_id_ = ft_remote_device_id;
        this->stream_id_ = stream_id;

        msg_listener_ = msg_notifier_->CreateListener();
        msg_listener_->Listen<SdkMsgTimer1000>([=, this](const auto& msg) {
            this->HeartBeat();
        });
    }

    NetClient::~NetClient() = default;

    void NetClient::Start() {
        if (network_type_ == ClientNetworkType::kWebsocket) {
            LOGI("Will connect by Websocket:");
            LOGI("media: {}", media_path_);
            LOGI("file transfer: {}", ft_path_);
            media_conn_ = std::make_shared<WsConnection>(sdk_params_, msg_notifier_, ip_, port_, media_path_);
            ft_conn_ = std::make_shared<WsConnection>(sdk_params_, msg_notifier_, ip_, port_, ft_path_);
        }
        else if (network_type_ == ClientNetworkType::kUdpKcp) {
            LOGI("Will connect by UDP");
            media_conn_ = std::make_shared<UdpConnection>(sdk_params_, msg_notifier_, ip_, port_);
        }
        else if (network_type_ == ClientNetworkType::kRelay) {
            auto auto_relay = !sdk_params_->enable_p2p_;
            media_conn_ = std::make_shared<RelayConnection>(sdk_params_, msg_notifier_,ip_, port_,device_id_,remote_device_id_,
                                                            auto_relay, kRoomTypeMedia);
            ft_conn_ = std::make_shared<RelayConnection>(sdk_params_, msg_notifier_, ip_, port_, ft_device_id_, ft_remote_device_id_,
                                                         auto_relay, kRoomTypeFileTransfer);

            if (sdk_params_->enable_p2p_) {
                auto relay_conn = std::dynamic_pointer_cast<RelayConnection>(media_conn_);
                rtc_conn_ = std::make_shared<WebRtcConnection>(relay_conn, sdk_params_, msg_notifier_);
            }
        }
        else {
            LOGE("Start failed! Don't know the connection type: {}", (int)network_type_);
            return;
        }

        media_conn_->RegisterOnConnectedCallback([=, this]() {
            if (conn_cbk_) {
                conn_cbk_();
            }
        });

        media_conn_->RegisterOnDisConnectedCallback([=, this]() {
            if (dis_conn_cbk_) {
                dis_conn_cbk_();
            }
        });

        media_conn_->RegisterOnMessageCallback([=, this](std::string&& data) {
            this->ParseMessage(data);

            // statistics
            this->stat_->AppendDataSize(data.size());
        });

        media_conn_->Start();
        if (ft_conn_) {
            ft_conn_->RegisterOnMessageCallback([=, this](std::string&& data) {
                this->ParseMessage(data);
                // statistics
                this->stat_->AppendDataSize(data.size());
            });
            ft_conn_->Start();
        }

        if (sdk_params_->enable_p2p_ && rtc_conn_) {
            rtc_conn_->SetOnMediaMessageCallback([=, this](const std::string& msg) {
                //LOGI("OnMediaMessageCallback, : {}", msg.size());
                this->ParseMessage(msg);

                // statistics
                this->stat_->AppendDataSize(msg.size());
            });
            rtc_conn_->SetOnFtMessageCallback([=, this](const std::string& msg) {
                this->ParseMessage(msg);
                this->stat_->AppendDataSize(msg.size());
            });
            rtc_conn_->Start();
        }
    }

    void NetClient::Exit() {
        if (media_conn_) {
            LOGI("Queued message count: {}", queuing_message_count_.load());
            media_conn_->Stop();
        }
        if (ft_conn_) {
            ft_conn_->Stop();
        }
        if (rtc_conn_) {
            rtc_conn_->Stop();
        }
        LOGI("WS has exited...");
    }

    void NetClient::ParseMessage(const std::string& msg) {
        auto net_msg = std::make_shared<tc::Message>();
        bool ok = net_msg->ParseFromArray(msg.data(), msg.size());
        if (!ok) {
            LOGE("Sdk ParseMessage failed.");
            return;
        }

        if (raw_msg_cbk_) {
            raw_msg_cbk_(net_msg);
        }

        if (net_msg->type() == tc::kVideoFrame) {
            const auto& video_frame = net_msg->video_frame();
            if (video_frame.key()) {
                //LOGI("video frame index: {}, {}x{}, key: {}", video_frame.frame_index(),
                //     video_frame.frame_width(), video_frame.frame_height(), video_frame.key());
            }
            if (video_frame_cbk_) {
                video_frame_cbk_(video_frame);
            }

        }
        else if (net_msg->type() == tc::kAudioFrame) {
            const auto& audio_frame = net_msg->audio_frame();
            if (audio_frame_cbk_) {
                audio_frame_cbk_(audio_frame);
            }
        }
        else if (net_msg->type() == tc::kCursorInfoSync) {
            const auto& cursor_info = net_msg->cursor_info_sync();
            if(cursor_info_sync_cbk_) {
                cursor_info_sync_cbk_(cursor_info);
            }
        }
        else if (net_msg->type() == tc::kRendererAudioSpectrum) {
            const auto& spectrum = net_msg->renderer_audio_spectrum();
            if (audio_spectrum_cbk_) {
                audio_spectrum_cbk_(spectrum);
            }
        }
        else if (net_msg->type() == tc::kOnHeartBeat) {
            const auto& hb = net_msg->on_heartbeat();
            if (hb_cbk_) {
                hb_cbk_(hb);
            }

            // calculate network delay
            auto send_timestamp = hb.timestamp();
            auto current_timestamp = TimeUtil::GetCurrentTimestamp();
            auto diff = current_timestamp - send_timestamp;
            stat_->AppendNetTimeDelay((int32_t)diff);

            // save render statistics
            auto& monitors_info = hb.monitors_info();
            for (const auto& [monitor_name, info] : monitors_info) {
                stat_->render_monitor_stat_[monitor_name] = info;
            }

            stat_->video_capture_type_ = hb.video_capture_type();
            stat_->audio_capture_type_ = hb.audio_capture_type();
            stat_->audio_encode_type_ = hb.audio_encode_type();

            stat_->remote_pc_info_ = hb.pc_info();
            stat_->remote_desktop_name_ = hb.desktop_name();
            stat_->remote_hd_info_ = hb.device_info();
            stat_->remote_os_name_ = hb.os_name();

        }
        else if (net_msg->type() == tc::kClipboardInfo) {
            if (clipboard_cbk_) {
                clipboard_cbk_(net_msg);
            }
        }
        else if (net_msg->type() == tc::kServerConfiguration) {
            const auto& config = net_msg->config();
            if (config_cbk_) {
                config_cbk_(config);
            }
        }
        else if (net_msg->type() == tc::kMonitorSwitched) {
            const auto& monitor_switched = net_msg->monitor_switched();
            if (monitor_switched_cbk_) {
                monitor_switched_cbk_(monitor_switched);
            }
        }
        else if (net_msg->type() == tc::kChangeMonitorResolutionResult) {
            auto sub = net_msg->change_monitor_resolution_result();
            msg_notifier_->SendAppMessage(SdkMsgChangeMonitorResolutionResult {
                .monitor_name_ = sub.monitor_name(),
                .result = sub.result(),
            });
        }
        else if (net_msg->type() == tc::kSigAnswerSdpMessage) {
            auto sub = net_msg->sig_answer_sdp();
            msg_notifier_->SendAppMessage(SdkMsgRemoteAnswerSdp {
                .answer_sdp_ = sub,
            });
        }
        else if (net_msg->type() == tc::kSigIceMessage) {
            auto sub = net_msg->sig_ice();
            msg_notifier_->SendAppMessage(SdkMsgRemoteIce {
                .ice_ = sub,
            });
        }
    }

    void NetClient::PostMediaMessage(const std::string& msg) {
        if (sdk_params_->enable_p2p_ && rtc_conn_ && rtc_conn_->IsMediaChannelReady()) {
            auto queuing_msg_count = rtc_conn_->GetQueuingMediaMsgCount();
            auto has_enough_buffer = rtc_conn_->HasEnoughBufferForQueuingMediaMessages();
            int wait_count = 0;
            while (queuing_msg_count >= kMaxQueuingFtMessages || !has_enough_buffer) {
                if (!rtc_conn_->IsMediaChannelReady()) {
                    return;
                }
                TimeUtil::DelayByCount(1);
                queuing_msg_count = rtc_conn_->GetQueuingMediaMsgCount();
                has_enough_buffer = rtc_conn_->HasEnoughBufferForQueuingMediaMessages();
                wait_count++;
            }
            if (wait_count > 0) {
                LOGI("===> [Media] wait for {}ms", wait_count);
            }

            rtc_conn_->PostMediaMessage(msg);
        }
        else {
            auto queuing_msg_count = this->GetQueuingMediaMsgCount();
            int wait_count = 0;
            while (queuing_msg_count >= kMaxQueuingFtMessages) {
                //LOGI("===> queue too many msgs, count: {}, wait for 1ms", queuing_msg_count);
                TimeUtil::DelayByCount(1);
                queuing_msg_count = this->GetQueuingMediaMsgCount();
                wait_count++;
            }

            if (media_conn_) {
                media_conn_->PostBinaryMessage(msg);
            }
        }
    }

    void NetClient::PostFileTransferMessage(const std::string& msg) {
        if (sdk_params_->enable_p2p_ && rtc_conn_ && rtc_conn_->IsFtChannelReady()) {
            auto queuing_msg_count = rtc_conn_->GetQueuingFtMsgCount();
            auto has_enough_buffer = rtc_conn_->HasEnoughBufferForQueuingFtMessages();
            int wait_count = 0;
            while (queuing_msg_count >= kMaxQueuingFtMessages || !has_enough_buffer) {
                if (!rtc_conn_->IsFtChannelReady()) {
                    return;
                }
                TimeUtil::DelayByCount(1);
                queuing_msg_count = rtc_conn_->GetQueuingFtMsgCount();
                has_enough_buffer = rtc_conn_->HasEnoughBufferForQueuingFtMessages();
                wait_count++;
            }
            if (wait_count > 0) {
                LOGI("===> [File] wait for {}ms", wait_count);
            }

            rtc_conn_->PostFtMessage(msg);
        }
        else {
            // TODO:
            auto queuing_msg_count = this->GetQueuingFtMsgCount();
            int wait_count = 0;
            while (queuing_msg_count >= kMaxQueuingFtMessages) {
                //LOGI("===> queue too many msgs, count: {}, wait for 1ms", queuing_msg_count);
                TimeUtil::DelayByCount(1);
                queuing_msg_count = this->GetQueuingFtMsgCount();
                wait_count++;
            }

            if (ft_conn_) {
                ft_conn_->PostBinaryMessage(msg);
            }
        }
    }

    void NetClient::SetOnVideoFrameMsgCallback(OnVideoFrameMsgCallback&& cbk) {
        video_frame_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnAudioFrameMsgCallback(OnAudioFrameMsgCallback&& cbk) {
        audio_frame_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnCursorInfoSyncMsgCallback(OnCursorInfoSyncMsgCallback&& cbk) {
        cursor_info_sync_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnConnectCallback(OnConnectedCallback&& cbk) {
        conn_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnDisconnectedCallback(OnDisconnectedCallback&& cbk) {
        dis_conn_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnAudioSpectrumCallback(OnAudioSpectrumCallback&& cbk) {
        audio_spectrum_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnHeartBeatCallback(tc::OnHeartBeatInfoCallback&& cbk) {
        hb_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnClipboardCallback(OnClipboardInfoCallback&& cbk) {
        clipboard_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnServerConfigurationCallback(tc::OnConfigCallback&& cbk) {
        config_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnMonitorSwitchedCallback(OnMonitorSwitchedCallback&& cbk) {
        monitor_switched_cbk_ = std::move(cbk);
    }

    void NetClient::SetOnRawMessageCallback(tc::OnRawMessageCallback&& cbk) {
        raw_msg_cbk_ = std::move(cbk);
    }

    void NetClient::HeartBeat() {
        auto msg = std::make_shared<Message>();
        msg->set_type(tc::kHeartBeat);
        msg->set_device_id(device_id_);
        msg->set_stream_id(stream_id_);
        auto hb = msg->mutable_heartbeat();
        hb->set_index(hb_idx_++);
        hb->set_timestamp((int64_t)TimeUtil::GetCurrentTimestamp());
        auto proto_msg = msg->SerializeAsString();
        this->PostMediaMessage(proto_msg);
    }

    int64_t NetClient::GetQueuingMediaMsgCount() {
        if (sdk_params_->enable_p2p_ && rtc_conn_) {
            return rtc_conn_->GetQueuingMediaMsgCount();
        }
        else if (media_conn_) {
            return media_conn_->GetQueuingMsgCount();
        }
        else {
            return 0;
        }
    }

    int64_t NetClient::GetQueuingFtMsgCount() {
        if (sdk_params_->enable_p2p_ && rtc_conn_) {
            return rtc_conn_->GetQueuingFtMsgCount();
        }
        else if (ft_conn_) {
            return ft_conn_->GetQueuingMsgCount();
        }
        else {
            return 0;
        }
    }

    void NetClient::On16msTimeout() {
        if (sdk_params_->enable_p2p_ && rtc_conn_) {
            rtc_conn_->On16msTimeout();
        }
        if (ft_conn_) {
            ft_conn_->On16msTimeout();
        }
        if (media_conn_) {
            media_conn_->On16msTimeout();
        }
    }

    void NetClient::RetryConnection() {
        if (media_conn_) {
            media_conn_->RetryConnection();
        }
        if (ft_conn_) {
            ft_conn_->RetryConnection();
        }
        if (rtc_conn_) {
            rtc_conn_->RetryConnection();
        }
    }
}