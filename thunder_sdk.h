//
// Created by hy on 2023/12/26.
//

#ifndef TC_CLIENT_PC_THUNDERSDK_H
#define TC_CLIENT_PC_THUNDERSDK_H

#include <iostream>
#include <string>

#include "tc_message.pb.h"

namespace tc
{
    class Data;
    class Thread;
    class WSClient;

    class ThunderSdkParams {
    public:
        [[nodiscard]] std::string MakeReqPath() const;

    public:
        bool ssl_ = false;
        std::string ip_;
        int port_;
        std::string req_path_;
    };

    class ThunderSdk {
    public:

        static std::shared_ptr<ThunderSdk> Make();

        ThunderSdk();
        ~ThunderSdk();

        bool Init(const ThunderSdkParams& params);
        void Start();
        void Exit();

    private:


    private:

        ThunderSdkParams sdk_params_;
        std::shared_ptr<WSClient> ws_client_ = nullptr;

    };

}

#endif //TC_CLIENT_PC_THUNDERSDK_H
