//
// Created by hy on 2023/12/26.
//

#include "thunder_sdk.h"

#include "tc_common/log.h"
#include "tc_common/file.h"

#include "ws_client.h"

namespace tc
{

    std::string ThunderSdkParams::MakeReqPath() const {
        auto base_url = ip_ + ":" + std::to_string(port_) + req_path_;
        return ssl_ ? "wss://" +  base_url : "ws://" + base_url;
    }

    ///

    std::shared_ptr<ThunderSdk> ThunderSdk::Make() {
        return std::make_shared<ThunderSdk>();
    }

    ThunderSdk::ThunderSdk() {

    }

    ThunderSdk::~ThunderSdk() {

    }

    bool ThunderSdk::Init(const ThunderSdkParams& params) {
        sdk_params_ = params;

        return true;
    }

    void ThunderSdk::Start() {
        // websocket client
        ws_client_ = WSClient::Make(sdk_params_.MakeReqPath());
        ws_client_->Start();


    }

    void ThunderSdk::Exit() {

    }

}