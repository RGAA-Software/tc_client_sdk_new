//
// Created by RGAA on 2024/1/26.
//

#ifndef TC_CLIENT_ANDROID_VIDEO_DECODER_FACTORY_H
#define TC_CLIENT_ANDROID_VIDEO_DECODER_FACTORY_H

#include "ffmpeg_video_decoder.h"
#include "mediacodec_video_decoder.h"

namespace tc
{

    enum SupportedCodec {
        kFFmpeg,
        kMediaCodec
    };

    class VideoDecoderFactory {
    public:

        static std::shared_ptr<VideoDecoder> Make(const SupportedCodec& codec) {
            if (codec == SupportedCodec::kFFmpeg) {
                return FFmpegVideoDecoder::Make();
            }
#ifdef ANDROID
            if (codec == SupportedCodec::kMediaCodec) {
                return MediacodecVideoDecoder::Make();
            }
#endif
            return nullptr;
        }

    };

}

#endif //TC_CLIENT_ANDROID_VIDEO_DECODER_FACTORY_H
