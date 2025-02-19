//
// Created by RGAA on 2024/1/26.
//

#ifndef TC_CLIENT_ANDROID_MEDIACODEC_VIDEO_DECODER_H
#define TC_CLIENT_ANDROID_MEDIACODEC_VIDEO_DECODER_H
#ifdef ANDROID

#include <memory>
#include <functional>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>

#include "video_decoder.h"

namespace tc
{

    class MediacodecVideoDecoder : public VideoDecoder {
    public:
        static std::shared_ptr<MediacodecVideoDecoder> Make();

        MediacodecVideoDecoder();
        ~MediacodecVideoDecoder() override;

        int Init(int codec_type, int width, int height, const std::string& frame, void* surface) override;
        int Decode(const uint8_t* data, int size, DecodedCallback&& cbk) override;
        void Release() override;
        bool Ready() override;
    private:

        AMediaCodec* media_codec_ = nullptr;
        AMediaFormat* media_format_ = nullptr;
        bool use_oes_ = false;

    };
}

#endif // ANDROID
#endif //TC_CLIENT_ANDROID_MEDIACODEC_VIDEO_DECODER_H
