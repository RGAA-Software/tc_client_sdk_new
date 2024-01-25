//
// Created by hy on 2024/1/25.
//

#ifndef TC_CLIENT_ANDROID_SDK_MESSAGES_H
#define TC_CLIENT_ANDROID_SDK_MESSAGES_H

#include <string>
#include <memory>

namespace tc
{

    class RawImage;

    //
    class MsgFirstFrameDecoded {
    public:
        std::shared_ptr<RawImage> raw_image_ = nullptr;
    };

    //
    class MsgFrameDecoded {
    public:
        uint64_t frame_idx_;
        std::shared_ptr<RawImage> raw_image_ = nullptr;
    };
}

#endif //TC_CLIENT_ANDROID_SDK_MESSAGES_H
