//
// Created by hy on 2024/1/26.
//

#ifndef TC_CLIENT_ANDROID_MEDIACODEC_VIDEO_DECODER_H
#define TC_CLIENT_ANDROID_MEDIACODEC_VIDEO_DECODER_H
#ifdef ANDROID

#include <memory>
#include <functional>

#include "video_decoder.h"

namespace tc
{

    class MediacodecVideoDecoder : public VideoDecoder {
    public:
        static std::shared_ptr<MediacodecVideoDecoder> Make();

        MediacodecVideoDecoder();
        ~MediacodecVideoDecoder() override;

        int Init(int codec_type, int width, int height) override;
        int Decode(const std::shared_ptr<Data>& frame, DecodedCallback&& cbk) override;
        int Decode(const std::string& frame, DecodedCallback&& cbk) override;
        int Decode(const uint8_t* data, int size, DecodedCallback&& cbk) override;
        void Release() override;

    };
}

#endif // ANDROID
#endif //TC_CLIENT_ANDROID_MEDIACODEC_VIDEO_DECODER_H