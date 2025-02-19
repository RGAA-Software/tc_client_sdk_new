//
// Created by RGAA on 2023/8/11.
//

#ifndef SAILFISH_CLIENT_PC_FFMPEGVIDEODECODER_H
#define SAILFISH_CLIENT_PC_FFMPEGVIDEODECODER_H

extern "C" {

    #include <libavcodec/codec.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libavutil/log.h>
}

#include "video_decoder.h"

namespace tc {

    class FFmpegVideoDecoder : public VideoDecoder {
    public:

        static std::shared_ptr<FFmpegVideoDecoder> Make();

        explicit FFmpegVideoDecoder();
        ~FFmpegVideoDecoder() override;

        int Init(int codec_type, int width, int height, const std::string& frame, void* surface) override;
        int Decode(const uint8_t* data, int size, DecodedCallback&& cbk) override;
        void Release() override;
        bool Ready() override;
        void EnableToRGBFormat();

    private:
        void ListCodecs();

    private:

        AVCodecContext* codec_context = nullptr;
        AVCodec* codec = nullptr;
        AVPacket* packet = nullptr;
        AVFrame* av_frame = nullptr;

        std::shared_ptr<RawImage> decoded_image_ = nullptr;

        bool cvt_to_rgb_ = false;

    };

}

#endif //SAILFISH_CLIENT_PC_FFMPEGVIDEODECODER_H
