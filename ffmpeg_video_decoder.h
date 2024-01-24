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

#include <memory>
#include <functional>
#include <mutex>

namespace tc {

    class Data;
    class RawImage;

    using DecodedCallback = std::function<void(const std::shared_ptr<RawImage>)>;

    class FFmpegVideoDecoder {
    public:

        static std::shared_ptr<FFmpegVideoDecoder> Make(bool to_rgb = false);

        explicit FFmpegVideoDecoder(bool to_rgb = false);
        ~FFmpegVideoDecoder();

        int Init(int codec_type, int width, int height);
        int Decode(const std::shared_ptr<Data>& frame, DecodedCallback&& cbk);
        int Decode(const std::string& frame, DecodedCallback&& cbk);
        int Decode(const uint8_t* data, int size, DecodedCallback&& cbk);
        void Release();

        bool NeedReConstruct(int codec_type, int width, int height);

    private:
        void ListCodecs();

    private:

        bool inited_ = false;

        int codec_type_ = -1;
        int frame_width_ = 0;
        int frame_height_ = 0;
        bool stop_ = false;
        AVCodecContext* codec_context = nullptr;
        AVCodec* codec = nullptr;
        AVPacket* packet = nullptr;
        AVFrame* av_frame = nullptr;

        std::shared_ptr<RawImage> decoded_image_ = nullptr;

        bool cvt_to_rgb = false;

        std::mutex decode_mtx_;

    };

}

#endif //SAILFISH_CLIENT_PC_FFMPEGVIDEODECODER_H
