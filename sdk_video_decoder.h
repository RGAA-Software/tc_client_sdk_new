//
// Created by RGAA on 2024/1/26.
//

#ifndef TC_CLIENT_ANDROID_VIDEO_DECODER_H
#define TC_CLIENT_ANDROID_VIDEO_DECODER_H

#include <memory>
#include <functional>
#include <mutex>

namespace tc
{
    class Data;
    class RawImage;

    using DecodedCallback = std::function<void(const std::shared_ptr<RawImage>)>;

    class VideoDecoder {
    public:
        VideoDecoder();
        virtual ~VideoDecoder();

        virtual int Init(int codec_type, int width, int height, const std::string& frame, void* surface, int img_format) = 0;
        virtual int Decode(const std::shared_ptr<Data>& frame, DecodedCallback&& cbk);
        virtual int Decode(const std::string& frame, DecodedCallback&& cbk);
        virtual int Decode(const uint8_t* data, int size, DecodedCallback&& cbk) = 0;
        virtual void Release();
        virtual bool NeedReConstruct(int codec_type, int width, int height, int img_format);
        virtual bool Ready() = 0;

    protected:
        bool inited_ = false;
        int codec_type_ = -1;
        int frame_width_ = 0;
        int frame_height_ = 0;
        int img_format_ = -1;

        bool stop_ = false;
        std::mutex decode_mtx_;
    };

}

#endif //TC_CLIENT_ANDROID_VIDEO_DECODER_H
