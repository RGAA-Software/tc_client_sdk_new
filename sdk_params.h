//
// Created by RGAA on 16/04/2025.
//

#ifndef GAMMARAY_SDK_PARAMS_H
#define GAMMARAY_SDK_PARAMS_H

#include "tc_message.pb.h"

namespace tc
{

    class ThunderSdkParams {
    public:
        [[nodiscard]] std::string MakeReqPath() const;

    public:
        bool ssl_ = false;
        bool enable_audio_ = false;
        bool enable_video_ = false;
        bool enable_controller_ = false;
        std::string ip_;
        int port_;
        std::string media_path_;
        std::string ft_path_;
        ClientType client_type_;
        //ClientConnectType conn_type_;
        ClientNetworkType nt_type_;
        // id only: xxxxx
        std::string bare_device_id_;
        // id only: xxxxx
        std::string bare_remote_device_id_;
        // client_xxxx_xxxx
        std::string device_id_;
        // server_xxxx
        std::string remote_device_id_;
        std::string ft_device_id_;
        std::string ft_remote_device_id_;
        std::string stream_id_;
        std::string stream_name_;
        bool enable_p2p_ = false;
        std::string display_name_;
        std::string display_remote_name_;

        //语言种类, 切换语言的时候会用到
        int language_id_ = 0;
    };

}

#endif //GAMMARAY_SDK_PARAMS_H
